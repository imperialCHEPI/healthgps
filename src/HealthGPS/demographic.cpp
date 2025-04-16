#include "demographic.h"
#include "HealthGPS.Core/thread_util.h"
#include "converter.h"
#include "person.h"
#include "runtime_context.h"
#include "static_linear_model.h"
#include "sync_message.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <mutex>
#include <iostream>

#include <oneapi/tbb/parallel_for_each.h>
using namespace hgps;

namespace { // anonymous namespace

/// @brief Defines the residual mortality synchronisation message
using ResidualMortalityMessage = hgps::SyncDataMessage<hgps::GenderTable<int, double>>;

} // anonymous namespace

namespace hgps {

DemographicModule::DemographicModule(
    std::map<int, std::map<int, PopulationRecord>> &&pop_data, LifeTable &&life_table,
    std::unordered_map<core::Identifier,
                       std::unordered_map<core::Gender, std::unordered_map<core::Region, double>>>
        region_prevalence,
    std::unordered_map<
        core::Identifier,
        std::unordered_map<core::Gender, std::unordered_map<core::Ethnicity, double>>>
        ethnicity_prevalence,
    std::unordered_map<core::Income, LinearModelParams> income_models)
    : pop_data_{std::move(pop_data)}, life_table_{std::move(life_table)},
      region_prevalence_{std::move(region_prevalence)},
      ethnicity_prevalence_{std::move(ethnicity_prevalence)},
      income_models_{std::move(income_models)} {
    if (pop_data_.empty()) {
        if (!life_table_.empty()) {
            throw std::invalid_argument("empty population and life table content mismatch.");
        }

        return;
    }

    if (life_table_.empty()) {
        throw std::invalid_argument("population and empty life table content mismatch.");
    }

    auto first_entry = pop_data_.begin();
    auto time_range = core::IntegerInterval(first_entry->first, pop_data_.rbegin()->first);
    core::IntegerInterval age_range{};
    if (!first_entry->second.empty()) {
        age_range = core::IntegerInterval(first_entry->second.begin()->first,
                                          first_entry->second.rbegin()->first);
    }

    if (time_range != life_table_.time_limits()) {
        throw std::invalid_argument("Population and life table time limits mismatch.");
    }

    if (age_range != life_table_.age_limits()) {
        throw std::invalid_argument("Population and life table age limits mismatch.");
    }

    initialise_birth_rates();
}

SimulationModuleType DemographicModule::type() const noexcept {
    return SimulationModuleType::Demographic;
}

const std::string &DemographicModule::name() const noexcept { return name_; }

std::size_t DemographicModule::get_total_population_size(int time_year) const noexcept {
    auto total = 0.0f;
    if (pop_data_.contains(time_year)) {
        const auto &year_data = pop_data_.at(time_year);
        total = std::accumulate(year_data.begin(), year_data.end(), 0.0f,
                                [](const float previous, const auto &element) {
                                    return previous + element.second.total();
                                });
    }

    return static_cast<std::size_t>(total);
}

double DemographicModule::get_total_deaths(int time_year) const noexcept {
    if (life_table_.contains_time(time_year)) {
        return life_table_.get_total_deaths_at(time_year);
    }

    return 0.0;
}

const std::map<int, PopulationRecord> &
DemographicModule::get_population_distribution(int time_year) const {
    return pop_data_.at(time_year);
}

std::map<int, DoubleGenderValue>
DemographicModule::get_age_gender_distribution(int time_year) const noexcept {
    std::map<int, DoubleGenderValue> result;
    if (!pop_data_.contains(time_year)) {
        return result;
    }

    const auto &year_data = pop_data_.at(time_year);
    if (!year_data.empty()) {
        double total_ratio = 1.0 / get_total_population_size(time_year);

        for (const auto &age : year_data) {
            result.emplace(age.first, DoubleGenderValue(age.second.males * total_ratio,
                                                        age.second.females * total_ratio));
        }
    }

    return result;
}

DoubleGenderValue DemographicModule::get_birth_rate(int time_year) const noexcept {
    if (birth_rates_.contains(time_year)) {
        return DoubleGenderValue{birth_rates_(time_year, core::Gender::male),
                                 birth_rates_(time_year, core::Gender::female)};
    }

    return DoubleGenderValue{0.0, 0.0};
}

// NOLINTNEXTLINE(bugprone-exception-escape)
double DemographicModule::get_residual_death_rate(int age, core::Gender gender) const noexcept {
    if (residual_death_rates_.contains(age)) {
        return residual_death_rates_.at(age, gender);
    }

    return 0.0;
}

// Made changes- Mahima
// Created a function to initialise age and gender using the same functionality as before just in a
// different function
void DemographicModule::initialise_age_gender(RuntimeContext &context) {
    auto age_gender_dist = get_age_gender_distribution(context.start_time());
    auto index = 0;
    auto pop_size = static_cast<int>(context.population().size());
    auto entry_total = static_cast<int>(age_gender_dist.size());
    for (auto entry_count = 1; auto &entry : age_gender_dist) {
        auto num_males = static_cast<int>(std::round(pop_size * entry.second.males));
        auto num_females = static_cast<int>(std::round(pop_size * entry.second.females));
        auto num_required = index + num_males + num_females;
        auto pop_diff = pop_size - num_required;
        // Final adjustment due to rounding errors
        if (entry_count == entry_total && pop_diff > 0) {
            auto half_diff = pop_diff / 2;
            num_males += half_diff;
            num_females += half_diff;
            if (entry.second.males > entry.second.females) {
                num_males += pop_diff - (half_diff * 2);
            } else {
                num_females += pop_diff - (half_diff * 2);
            }

            [[maybe_unused]] auto pop_diff_new = pop_size - (index + num_males + num_females);
            assert(pop_diff_new == 0);
        } else if (pop_diff < 0) {
            pop_diff *= -1;
            if (entry.second.males > entry.second.females) {
                num_females -= pop_diff;
                if (num_females < 0) {
                    num_males += num_females;
                }
            } else {
                num_males -= pop_diff;
                if (num_males < 0) {
                    num_females += num_males;
                }
            }

            num_males = std::max(0, num_males);
            num_females = std::max(0, num_females);

            [[maybe_unused]] auto pop_diff_new = pop_size - (index + num_males + num_females);
            assert(pop_diff_new == 0);
        }

        // [index, index + num_males)
        for (auto i = 0; i < num_males; i++) {
            context.population()[index].age = entry.first;
            context.population()[index].gender = core::Gender::male;
            index++;
        }

        // [index + num_males, num_required)
        for (auto i = 0; i < num_females; i++) {
            context.population()[index].age = entry.first;
            context.population()[index].gender = core::Gender::female;
            index++;
        }

        entry_count++;
    }

    assert(index == pop_size);
    std::cout << "\nDEBUG: Finished assigning age and gender";
}

// Made structural change- Mahima
// This function now is used to initialise population with age, gender, region, ethncicty, income
void DemographicModule::initialise_population(RuntimeContext &context) {
    // First initialize everyone's age and gender (ONCE)
    initialise_age_gender(context);
    
    // Then initialize other attributes for each person
    for (auto &person : context.population()) {
        // STEP-2 Initialize region
        // STEP-3 Initialize ethnicity
        // STEP-4 Initialize income continuous
        // STEP-5 Initialize income category
        // STEP-6 Initialize physical activity
        if (person.is_active()) {
            initialise_region(context, person, context.random());
            initialise_ethnicity(context, person, context.random());
            initialise_income_continuous(context, person, context.random());
            const Population &population = context.population();
            initialise_income_category(person, population);
        }
    }
}

// Population-level initialization functions
void DemographicModule::initialise_region([[maybe_unused]] RuntimeContext &context, Person &person,
                                          Random &random) {
    //std::cout << "\nDEBUG: Inside initialise_region";
    
    // Determine the age group for this person
    core::Identifier age_group = person.age < 18 ? "Under18"_id : "Over18"_id;
    
    // Check if the age_group exists in region_prevalence_
    if (!region_prevalence_.contains(age_group)) {
        std::cout << "\nDEBUG: ERROR - Age group not found in region_prevalence_ map: " << age_group.to_string() << std::endl;
        // Assign a default region to prevent crash
        person.region = core::Region::England;
        return;
    }
    
    // Check if the gender exists for this age_group
    if (!region_prevalence_.at(age_group).contains(person.gender)) {
        std::cout << "\nDEBUG: ERROR - Gender not found in region_prevalence_ for age group: " << age_group.to_string() << std::endl;
        // Assign a default region to prevent crash
        person.region = core::Region::England;
        return;
    }

    // Get region probabilities directly from the stored data
    const auto &region_probs = region_prevalence_.at(age_group).at(person.gender);
    double sum_probs = 0.0;
    for (const auto &[region, prob] : region_probs) {

        sum_probs += prob;
    }

    // Use CDF for assignment
    double rand_value = random.next_double(); // next_double is always between 0,1
    //std::cout << "\nDEBUG: Random value for region assignment: " << rand_value << std::endl;
    
    double cumulative_prob = 0.0;

    for (const auto &[region, prob] : region_probs) {
        cumulative_prob += prob;
        //std::cout << "\nDEBUG: Region: " << static_cast<int>(region) << ", Prob: " << prob  << ", Cumulative: " << cumulative_prob << ", Random: " << rand_value << std::endl;
        
        if (rand_value < cumulative_prob) { // Changed from <= to < for correct CDF sampling
            person.region = region;
            return;
        }
    }
    
    std::cout << "\nDEBUG: WARNING - Reached end of region assignment loop without assigning a region" << std::endl;
    
    // If we reach here, no region was assigned - this indicates an error in probability
    // distribution
    throw core::HgpsException("Failed to assign region: cumulative probabilities do not sum to "
                                  "1.0 or are incorrectly distributed");
   
}

void DemographicModule::initialise_ethnicity([[maybe_unused]] RuntimeContext &context,
                                             Person &person, Random &random) {
    //std::cout << "\nDEBUG: Inside initialise_ethnicity";
    
    // Determine the age group for this person
    core::Identifier age_group = person.age < 18 ? "Under18"_id : "Over18"_id;

    // Check if the age_group exists in ethnicity_prevalence_
    if (!ethnicity_prevalence_.contains(age_group)) {
        std::cout << "\nDEBUG: ERROR - Age group not found in ethnicity_prevalence_ map: " << age_group.to_string() << std::endl;
    }
    
    // Check if the gender exists for this age_group
    if (!ethnicity_prevalence_.at(age_group).contains(person.gender)) {
        std::cout << "\nDEBUG: ERROR - Gender not found in ethnicity_prevalence_ for age group: " << age_group.to_string() << std::endl;
        // Assign a default ethnicity to prevent crash
        person.ethnicity = core::Ethnicity::White;
        return;
    }

    // Get ethnicity probabilities directly from the stored data
    const auto &ethnicity_probs = ethnicity_prevalence_.at(age_group).at(person.gender);

    double rand_value = random.next_double(); // next_double is between 0,1
    
    double cumulative_prob = 0.0;

    for (const auto &[ethnicity, prob] : ethnicity_probs) {
        cumulative_prob += prob;
        //std::cout << "\nDEBUG: Ethnicity: " << static_cast<int>(ethnicity) << ", Prob: " << prob << ", Cumulative: " << cumulative_prob << ", Random: " << rand_value << std::endl;
                  
        if (rand_value < cumulative_prob) {
            person.ethnicity = ethnicity;
            return;
        }
    }
    
    std::cout << "\nDEBUG: WARNING - Reached end of ethnicity assignment loop without assigning ethnicity" << std::endl;

    // If we reach here, no ethnicity was assigned - this indicates an error in probability
    // distribution
    throw core::HgpsException("Failed to assign ethnicity: cumulative probabilities do not sum to 1.0 or are incorrectly distributed");
}

void DemographicModule::initialise_income_continuous([[maybe_unused]] RuntimeContext &context,
                                                     Person &person, Random &random) {
    // income_continuous is considered as household income and assigned to every person
    // Find the income model to use (there should be only one)
    //std::cout << "\nDEBUG: Inside initialise_income_continuous";
    if (!income_models_.empty()) {
        const auto &model_pair = *income_models_.begin(); // Get the first model regardless of key
        const auto &model = model_pair.second;

        // Start with the intercept
        double value = model.intercept;
        //std::cout << "\nDEBUG: Found intercept value of:" << value;

        // Directly apply coefficients based on person's attributes
        for (const auto &[factor_name, coefficient] : model.coefficients) {
            // Skip the standard deviation entry as it's not a factor
            if (factor_name == "IncomeContinuousStdDev")
                continue;
                
            // Age effects
            if (factor_name == "Age") {
                value += coefficient * static_cast<double>(person.age);
            }
            else if (factor_name == "Age2") {
                value += coefficient * pow(person.age, 2);
            }
            else if (factor_name == "Age3") {
                value += coefficient * pow(person.age, 3);
            }
            // Gender effect
            else if (factor_name == "Gender") {
                // Male = 1, Female = 0
                if (person.gender == core::Gender::male) {
                    value += coefficient;
                }
            }
            // Sector effect
            else if (factor_name == "Sector") {
                // Rural = 1, Urban = 0
                if (person.sector == core::Sector::rural) {
                    value += coefficient;
                }
            }
            // Region effects - only apply if the region matches
            else if (factor_name == "England" && person.region == core::Region::England) {
                value += coefficient;
            }
            else if (factor_name == "Wales" && person.region == core::Region::Wales) {
                value += coefficient;
            }
            else if (factor_name == "Scotland" && person.region == core::Region::Scotland) {
                value += coefficient;
            }
            else if (factor_name == "NorthernIreland" && person.region == core::Region::NorthernIreland) {
                value += coefficient;
            }
            // Ethnicity effects - only apply if the ethnicity matches
            else if (factor_name == "White" && person.ethnicity == core::Ethnicity::White) {
                value += coefficient;
            }
            else if (factor_name == "Black" && person.ethnicity == core::Ethnicity::Black) {
                value += coefficient;
            }
            else if (factor_name == "Asian" && person.ethnicity == core::Ethnicity::Asian) {
                value += coefficient;
            }
            else if (factor_name == "Mixed" && person.ethnicity == core::Ethnicity::Mixed) {
                value += coefficient;
            }
            else if ((factor_name == "Others" || factor_name == "Other") && 
                    person.ethnicity == core::Ethnicity::Other) {
                value += coefficient;
            }
        }

        // Get the standard deviation from the model
        double income_stddev = 0.0;
        if (model.coefficients.count("IncomeContinuousStdDev") > 0) {
            income_stddev = model.coefficients.at("IncomeContinuousStdDev");
        }

        // Add random noise
        double noise = random.next_normal(0.0, income_stddev);
        double final_value = value * (1.0 + noise);

        // Set the income_continuous value
        person.income_continuous = final_value;
    }
    //std::cout << "\nDEBUG: Finished initialise_income_continuous";
}

void DemographicModule::initialise_income_category(Person &person, const Population &population) {
    // Apply the income category based on the person's income_continuous value and current
    // thresholds
    //std::cout << "\nDEBUG: Inside initialise_income_category";
    if (income_quartile_thresholds_.empty()) {
        calculate_income_quartiles(population);
    }

    double income_value = person.income_continuous;

    if (income_value <= income_quartile_thresholds_[0]) {
        person.income = core::Income::low;
    } else if (income_value <= income_quartile_thresholds_[1]) {
        person.income = core::Income::lowermiddle;
    } else if (income_value <= income_quartile_thresholds_[2]) {
        person.income = core::Income::uppermiddle;
    } else {
        person.income = core::Income::high;
    }
    //std::cout << "\nDEBUG: Finished initialise_income_category";
}

void DemographicModule::calculate_income_quartiles(const Population &population) {
    // Collect all valid income values
    std::vector<double> sorted_incomes;
    sorted_incomes.reserve(population.size());
    //std::cout << "\nDEBUG: Inside calculating quartiles and sorted done";

    for (const auto &p : population) {
        if (p.is_active() && p.income_continuous > 0) {
            sorted_incomes.push_back(p.income_continuous);
        }
    }

    // Sort to find quartile thresholds
    std::sort(sorted_incomes.begin(), sorted_incomes.end());

    size_t n = sorted_incomes.size();
    income_quartile_thresholds_.resize(3);
    income_quartile_thresholds_[0] = sorted_incomes[n / 4];     // 25th percentile
    income_quartile_thresholds_[1] = sorted_incomes[n / 2];     // 50th percentile
    income_quartile_thresholds_[2] = sorted_incomes[3 * n / 4]; // 75th percentile

    //std::cout << "\nDEBUG: Finsihed calculating quartiles- gave data to initialise_income_category";
}

void DemographicModule::update_population(RuntimeContext &context,
                                          const DiseaseModule &disease_host) {
    auto residual_future = core::run_async(&DemographicModule::update_residual_mortality, this,
                                           std::ref(context), std::ref(disease_host));

    auto initial_pop_size = context.population().current_active_size();
    auto expected_pop_size = get_total_population_size(context.time_now());
    auto expected_num_deaths = get_total_deaths(context.time_now());

    // apply death events and update basic information (age)
    residual_future.get();
    auto number_of_deaths = update_age_and_death_events(context, disease_host);

    // Update demographic variables including income categories
    update_income_category(context);

    // apply births events
    auto last_year_births_rate = get_birth_rate(context.time_now() - 1);
    auto number_of_boys = static_cast<int>(last_year_births_rate.males * initial_pop_size);
    auto number_of_girls = static_cast<int>(last_year_births_rate.females * initial_pop_size);
    context.population().add_newborn_babies(number_of_boys, core::Gender::male, context.time_now());
    context.population().add_newborn_babies(number_of_girls, core::Gender::female,
                                            context.time_now());

    // Initialize demographic variables for newborns
    initialize_newborns(context);

    // Calculate statistics.
    auto simulated_death_rate = number_of_deaths * 1000.0 / initial_pop_size;
    auto expected_death_rate = expected_num_deaths * 1000.0 / expected_pop_size;
    auto percent_difference = 100 * (simulated_death_rate / expected_death_rate - 1);
    context.metrics()["SimulatedDeathRate"] = simulated_death_rate;
    context.metrics()["ExpectedDeathRate"] = expected_death_rate;
    context.metrics()["DeathRateDeltaPercent"] = percent_difference;
}

void DemographicModule::update_income_category(RuntimeContext &context) {
    // Income category is updated every 5 years
    static int last_update_year = 0;
    int current_year = context.time_now();

    // Only update every 5 years to reduce computational cost
    if (current_year - last_update_year >= 5) {
        // First recalculate the income quartiles based on current population
        calculate_income_quartiles(context.population());

        // Then update everyone's income category
        for (auto &person : context.population()) {
            if (person.is_active()) {
                initialise_income_category(person, context.population());
            }
        }
        last_update_year = current_year;
    }
}

void DemographicModule::update_residual_mortality(RuntimeContext &context,
                                                  const DiseaseModule &disease_host) {
    if (context.scenario().type() == ScenarioType::baseline) {
        auto residual_mortality = calculate_residual_mortality(context, disease_host);
        residual_death_rates_ = residual_mortality;

        context.scenario().channel().send(std::make_unique<ResidualMortalityMessage>(
            context.current_run(), context.time_now(), std::move(residual_mortality)));
    } else {
        auto message = context.scenario().channel().try_receive(context.sync_timeout_millis());
        if (message.has_value()) {
            auto &basePtr = message.value();
            auto *messagePrt = dynamic_cast<ResidualMortalityMessage *>(basePtr.get());
            if (messagePrt) {
                residual_death_rates_ = messagePrt->data();
            } else {
                throw std::runtime_error(
                    "Simulation out of sync, failed to receive a residual mortality message");
            }
        } else {
            throw std::runtime_error(
                "Simulation out of sync, receive residual mortality message has timed out");
        }
    }
}

void DemographicModule::initialise_birth_rates() {
    birth_rates_ = create_integer_gender_table<double>(life_table_.time_limits());
    auto start_time = life_table_.time_limits().lower();
    auto end_time = life_table_.time_limits().upper();
    for (int year = start_time; year <= end_time; year++) {
        const auto &births = life_table_.get_births_at(year);
        auto population_size = get_total_population_size(year);

        double male_birth_rate =
            births.number * births.sex_ratio / (1.0f + births.sex_ratio) / population_size;
        double female_birth_rate = births.number / (1.0f + births.sex_ratio) / population_size;

        birth_rates_.at(year, core::Gender::male) = male_birth_rate;
        birth_rates_.at(year, core::Gender::female) = female_birth_rate;
    }
}

GenderTable<int, double> DemographicModule::create_death_rates_table(int time_year) {
    auto &population = pop_data_.at(time_year);
    const auto &mortality = life_table_.get_mortalities_at(time_year);
    auto death_rates = create_integer_gender_table<double>(life_table_.age_limits());
    auto start_age = life_table_.age_limits().lower();
    auto end_age = life_table_.age_limits().upper();
    for (int age = start_age; age <= end_age; age++) {
        auto male_rate = std::min(mortality.at(age).males / population.at(age).males, 1.0f);
        auto feme_rate = std::min(mortality.at(age).females / population.at(age).females, 1.0f);
        death_rates.at(age, core::Gender::male) = male_rate;
        death_rates.at(age, core::Gender::female) = feme_rate;
    }

    return death_rates;
}

GenderTable<int, double>
DemographicModule::calculate_residual_mortality(RuntimeContext &context,
                                                const DiseaseModule &disease_host) {
    auto excess_mortality_product = create_integer_gender_table<double>(life_table_.age_limits());
    auto excess_mortality_count = create_integer_gender_table<int>(life_table_.age_limits());
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};
    tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        auto product = calculate_excess_mortality_product(entity, disease_host);
        auto lock = std::unique_lock{sum_mutex};
        excess_mortality_product.at(entity.age, entity.gender) += product;
        excess_mortality_count.at(entity.age, entity.gender)++;
    });

    auto death_rates = create_death_rates_table(context.time_now());
    auto residual_mortality = create_integer_gender_table<double>(life_table_.age_limits());
    auto start_age = life_table_.age_limits().lower();
    auto end_age = life_table_.age_limits().upper();
    auto default_average = 1.0;
    for (int age = start_age; age <= end_age; age++) {
        auto male_average_product = default_average;
        auto female_average_product = default_average;
        auto male_count = excess_mortality_count.at(age, core::Gender::male);
        auto female_count = excess_mortality_count.at(age, core::Gender::female);
        if (male_count > 0) {
            male_average_product =
                excess_mortality_product.at(age, core::Gender::male) / male_count;
        }

        if (female_count > 0) {
            female_average_product =
                excess_mortality_product.at(age, core::Gender::female) / female_count;
        }

        auto male_mortality =
            1.0 - (1.0 - death_rates.at(age, core::Gender::male)) / male_average_product;
        auto feme_mortality =
            1.0 - (1.0 - death_rates.at(age, core::Gender::female)) / female_average_product;
        residual_mortality.at(age, core::Gender::male) =
            std::max(std::min(male_mortality, 1.0), 0.0);
        residual_mortality.at(age, core::Gender::female) =
            std::max(std::min(feme_mortality, 1.0), 0.0);
    }

    return residual_mortality;
}

double DemographicModule::calculate_excess_mortality_product(const Person &entity,
                                                             const DiseaseModule &disease_host) {
    auto product = 1.0;
    for (const auto &item : entity.diseases) {
        if (item.second.status == DiseaseStatus::active) {
            auto excess_mortality = disease_host.get_excess_mortality(item.first, entity);
            product *= (1.0 - excess_mortality);
        }
    }

    return std::max(std::min(product, 1.0), 0.0);
}

int DemographicModule::update_age_and_death_events(RuntimeContext &context,
                                                   const DiseaseModule &disease_host) {
    auto max_age = static_cast<unsigned int>(context.age_range().upper());
    auto number_of_deaths = 0;
    for (auto &entity : context.population()) {
        if (!entity.is_active()) {
            continue;
        }

        if (entity.age >= max_age) {
            entity.die(context.time_now());
            number_of_deaths++;
        } else {

            // Calculate death probability based on the health status
            auto residual_death_rate = get_residual_death_rate(entity.age, entity.gender);
            auto product = 1.0 - residual_death_rate;
            for (const auto &item : entity.diseases) {
                if (item.second.status == DiseaseStatus::active) {
                    auto excess_mortality = disease_host.get_excess_mortality(item.first, entity);
                    product *= (1.0 - excess_mortality);
                }
            }

            auto death_probability = 1.0 - product;
            auto hazard = context.random().next_double();
            if (hazard < death_probability) {
                entity.die(context.time_now());
                number_of_deaths++;
            }
        }

        if (entity.is_active()) {
            entity.age = entity.age + 1;
        }
    }

    return number_of_deaths;
}

void DemographicModule::initialize_newborns(RuntimeContext &context) {
    // Initialize demographic variables for newborns (age == 0)
    for (auto &person : context.population()) {
        if (person.is_active() && person.age == 0) {
            initialise_region(context, person, context.random());
            initialise_ethnicity(context, person, context.random());
            initialise_income_continuous(context, person, context.random());
            initialise_income_category(person, context.population());
        }
    }
}

std::unique_ptr<DemographicModule> build_population_module(Repository &repository,
                                                           const ModelInput &config) {
    std::cout << "\nDEBUG: Starting build_population_module" << std::endl;
    
    // year => age [age, male, female]
    auto pop_data = std::map<int, std::map<int, PopulationRecord>>();

    auto min_time = config.start_time();
    auto max_time = config.stop_time();
    auto time_filter = [&min_time, &max_time](unsigned int value) {
        return value >= min_time && value <= max_time;
    };

    auto pop = repository.manager().get_population(config.settings().country(), time_filter);
    for (auto &item : pop) {
        pop_data[item.at_time].emplace(item.with_age,
                                       PopulationRecord(item.with_age, item.males, item.females));
    }

    auto births =
        repository.manager().get_birth_indicators(config.settings().country(), time_filter);
    auto deaths = repository.manager().get_mortality(config.settings().country(), time_filter);
    auto life_table = detail::StoreConverter::to_life_table(births, deaths);

    //std::cout << "\nDEBUG: About to get demographic configuration from static model" << std::endl;
    
    // Get demographic configuration from the static risk factor model
    const auto &static_model = dynamic_cast<const StaticLinearModelDefinition &>(
        repository.get_risk_factor_model_definition(RiskFactorModelType::Static));

    // Extract the configuration data using the getter methods
    auto region_prevalence = static_model.get_region_prevalence();
    
    auto ethnicity_prevalence = static_model.get_ethnicity_prevalence();
    auto income_models = static_model.get_income_models();

    // Extract physical activity parameters
    std::unordered_map<std::string, double> physical_activity_params;
    physical_activity_params["StandardDeviation"] = static_model.get_physical_activity_stddev();

    std::cout << "\nDEBUG: Creating DemographicModule with extracted configuration" << std::endl;
    
    // Create and return the demographic module with all the configuration data
    return std::make_unique<DemographicModule>(
        std::move(pop_data), std::move(life_table), std::move(region_prevalence),
        std::move(ethnicity_prevalence), std::move(income_models));
}
} // namespace hgps
