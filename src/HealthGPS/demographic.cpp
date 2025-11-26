#include "demographic.h"
#include "HealthGPS.Core/thread_util.h"
#include "converter.h"
#include "sync_message.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <fmt/format.h>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>

#include <oneapi/tbb/parallel_for_each.h>

namespace { // anonymous namespace

/// @brief Defines the residual mortality synchronisation message
using ResidualMortalityMessage = hgps::SyncDataMessage<hgps::GenderTable<int, double>>;

} // anonymous namespace

namespace hgps {

DemographicModule::DemographicModule(std::map<int, std::map<int, PopulationRecord>> &&pop_data,
                                     LifeTable &&life_table)
    : pop_data_{std::move(pop_data)}, life_table_{std::move(life_table)} {
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

void DemographicModule::initialise_population(RuntimeContext &context) {
    std::cout << "\n=== DEMOGRAPHIC MODULE: INITIALIZING POPULATION ===";
    auto age_gender_dist = get_age_gender_distribution(context.start_time());
    auto index = 0;
    auto pop_size = static_cast<int>(context.population().size());
    auto entry_total = static_cast<int>(age_gender_dist.size());
    std::cout << "\nPopulation size: " << pop_size << ", Age groups: " << entry_total;
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
            // STEP 1: Initilaize age
            // STEP 2: Initialize gender
            context.population()[index + i].age = entry.first;
            context.population()[index + i].gender = core::Gender::male;

            // STEP3: Initialize region
            // STEP 4: Initialize ethnicity
            initialise_region(context, context.population()[index + i], context.random());
            initialise_ethnicity(context, context.population()[index + i], context.random());
        }
        index += num_males;

        // [index + num_males, num_required)
        for (auto i = 0; i < num_females; i++) {
            context.population()[index + i].age = entry.first;
            context.population()[index + i].gender = core::Gender::female;

            // Initialize region and ethnicity in correct order
            initialise_region(context, context.population()[index + i], context.random());
            initialise_ethnicity(context, context.population()[index + i], context.random());
        }
        index += num_females;

        entry_count++;
    }

    assert(index == pop_size);
    std::cout << "\n=== DEMOGRAPHIC MODULE: POPULATION INITIALIZATION COMPLETED ===";
    std::cout << "\nDemographic initialization order: Age -> Gender -> Region -> Ethnicity";
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

    // apply births events
    auto last_year_births_rate = get_birth_rate(context.time_now() - 1);
    auto number_of_boys = static_cast<int>(last_year_births_rate.males * initial_pop_size);
    auto number_of_girls = static_cast<int>(last_year_births_rate.females * initial_pop_size);
    context.population().add_newborn_babies(number_of_boys, core::Gender::male, context.time_now());
    context.population().add_newborn_babies(number_of_girls, core::Gender::female,
                                            context.time_now());

    // Calculate statistics.
    auto simulated_death_rate = number_of_deaths * 1000.0 / initial_pop_size;
    auto expected_death_rate = expected_num_deaths * 1000.0 / expected_pop_size;
    auto percent_difference = 100 * (simulated_death_rate / expected_death_rate - 1);
    context.metrics()["SimulatedDeathRate"] = simulated_death_rate;
    context.metrics()["ExpectedDeathRate"] = expected_death_rate;
    context.metrics()["DeathRateDeltaPercent"] = percent_difference;
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void DemographicModule::initialise_region([[maybe_unused]] RuntimeContext &context, Person &person,
                                          Random &random) {
    static int region_count = 0;
    static bool first_call = true;
    region_count++;

    if (first_call) {
        std::cout << "\nStarting region initialization...";
        if (region_prevalence_.empty()) {
            std::cout << "\n  WARNING: No region data available (this is OK for India/PIF projects)";
        } else {
            std::cout << "\n  Region data available for " << region_prevalence_.size() << " age groups";
        }
        first_call = false;
    }
    // If no region data is available, skip region assignment
    if (region_prevalence_.empty()) {
        // Region data not available for this project (e.g., India/PIF) - leave as "unknown"
        return;
    }

    // Create an age-specific identifier in the format used in the CSV loading
    core::Identifier age_id("age_" + std::to_string(person.age));

    // Find the closest age in region_prevalence_ if exact age doesn't exist
    core::Identifier target_age_id = age_id;
    if (!region_prevalence_.contains(age_id)) {
        // Find closest age
        int min_diff = std::numeric_limits<int>::max();
        bool found_closest = false;
        for (const auto &[age_key, _] : region_prevalence_) {
            // Parse age from "age_X" format
            std::string age_str = age_key.to_string();
            if (age_str.substr(0, 4) == "age_") {
                int age_val = std::stoi(age_str.substr(4));
                int diff = std::abs(static_cast<int>(person.age) - age_val);
                if (diff < min_diff) {
                    min_diff = diff;
                    target_age_id = age_key;
                    found_closest = true;
                }
            }
        }
        if (!found_closest) {
            throw core::HgpsException(fmt::format(
                "No valid region data found for age: {}. Region data exists but contains no valid age keys.",
                person.age));
        }
    }

    // Check if the gender exists for this age
    if (!region_prevalence_.at(target_age_id).contains(person.gender)) {
        std::string gender_str = (person.gender == core::Gender::male) ? "male" : "female";
        throw core::HgpsException(fmt::format(
            "Gender '{}' not found in region_prevalence_ for age: {}. Available genders: {}",
            gender_str, target_age_id.to_string(), [this, &target_age_id]() {
                std::vector<std::string> genders;
                for (const auto &[g, _] : region_prevalence_.at(target_age_id)) {
                    genders.emplace_back((g == core::Gender::male) ? "male" : "female");
                }
                return fmt::format("[{}]", fmt::join(genders, ", "));
            }()));
    }

    // Get region probabilities directly from the stored data for this age
    const auto &region_probs = region_prevalence_.at(target_age_id).at(person.gender);

    // Use CDF for assignment
    double rand_value = random.next_double(); // next_double is always between 0 and 1
    double cumulative_prob = 0.0;

    for (const auto &[region_name, prob] : region_probs) {
        cumulative_prob += prob;
        if (rand_value < cumulative_prob) {
            person.region = region_name;
            return;
        }
    }

    // If we reach here, no region was assigned - this indicates an error in probability
    // distribution
    std::vector<std::string> region_names;
    std::vector<double> probs;
    for (const auto &[name, prob] : region_probs) {
        region_names.push_back(name);
        probs.push_back(prob);
    }

    throw core::HgpsException(fmt::format(
        "Failed to assign region: cumulative probabilities do not sum to 1.0. "
        "Age: {}, Gender: {}, Regions: {}, Probabilities: {}, Cumulative sum: {}",
        target_age_id.to_string(), (person.gender == core::Gender::male) ? "male" : "female",
        fmt::format("[{}]", fmt::join(region_names, ", ")),
        fmt::format("[{}]", fmt::join(probs, ", ")), cumulative_prob));

    if (region_count % 5000 == 0) {
        std::cout << "\nSuccessfully initialized region for " << region_count << " people";
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void DemographicModule::initialise_ethnicity([[maybe_unused]] RuntimeContext &context,
                                             Person &person, Random &random) {
    static int ethnicity_count = 0;
    static bool first_call = true;
    ethnicity_count++;

    if (first_call) {
        std::cout << "\nStarting ethnicity initialization...";
        if (ethnicity_prevalence_.empty()) {
            std::cout << "\n  WARNING: No ethnicity data available (this is OK for India/PIF projects)";
        } else {
            std::cout << "\n  Ethnicity data available for " << ethnicity_prevalence_.size() << " age groups";
        }
        first_call = false;
    }
    // If no ethnicity data is available, skip ethnicity assignment
    if (ethnicity_prevalence_.empty()) {
        // Ethnicity data not available for this project (e.g., India/PIF) - leave as "unknown"
        return;
    }

    // Determine the age group for this person
    // In the loading I'm assigning 0-under18 and 1-over18
    core::Identifier age_group = person.age < 18 ? "Under18"_id : "Over18"_id;

    // Check if the age_group exists in ethnicity_prevalence_
    if (!ethnicity_prevalence_.contains(age_group)) {
        std::vector<std::string> available_age_groups;
        available_age_groups.reserve(ethnicity_prevalence_.size());
        for (const auto &[ag, _] : ethnicity_prevalence_) {
            available_age_groups.emplace_back(ag.to_string());
        }

        throw core::HgpsException(fmt::format(
            "Age group '{}' not found in ethnicity_prevalence_ map. Available age groups: [{}]. "
            "Please ensure ethnicity CSV file contains data for all required age groups.",
            age_group.to_string(), fmt::join(available_age_groups, ", ")));
    }

    // Check if the gender exists for this age_group
    if (!ethnicity_prevalence_.at(age_group).contains(person.gender)) {
        std::string gender_str = (person.gender == core::Gender::male) ? "male" : "female";
        std::vector<std::string> available_genders;
        for (const auto &[g, _] : ethnicity_prevalence_.at(age_group)) {
            available_genders.emplace_back((g == core::Gender::male) ? "male" : "female");
        }

        throw core::HgpsException(fmt::format("Gender '{}' not found in ethnicity_prevalence_ for "
                                              "age group: {}. Available genders: [{}].",
                                              gender_str, age_group.to_string(),
                                              fmt::join(available_genders, ", ")));
    }

    // Check if the region exists for this age_group and gender
    if (!ethnicity_prevalence_.at(age_group).at(person.gender).contains(person.region)) {
        std::vector<std::string> available_regions;
        for (const auto &[r, _] : ethnicity_prevalence_.at(age_group).at(person.gender)) {
            available_regions.push_back(r);
        }

        throw core::HgpsException(fmt::format(
            "Region '{}' not found in ethnicity_prevalence_ for age group: {} and gender: {}. "
            "Available regions: [{}]. Please ensure ethnicity CSV file contains data for all "
            "regions.",
            person.region, age_group.to_string(),
            (person.gender == core::Gender::male) ? "male" : "female",
            fmt::join(available_regions, ", ")));
    }

    // Get ethnicity probabilities directly from the stored data
    const auto &ethnicity_probs =
        ethnicity_prevalence_.at(age_group).at(person.gender).at(person.region);

    double rand_value = random.next_double(); // next_double is between 0,1
    double cumulative_prob = 0.0;

    for (const auto &[ethnicity_name, prob] : ethnicity_probs) {
        cumulative_prob += prob;
        if (rand_value < cumulative_prob) {
            // Store the numeric ethnicity identifier that matches coefficient names
            // Convert ethnicity name to numeric identifier (ethnicity1, ethnicity2, etc.)
            if (ethnicity_name == "1") {
                person.ethnicity = "ethnicity1";
            } else if (ethnicity_name == "2") {
                person.ethnicity = "ethnicity2";
            } else if (ethnicity_name == "3") {
                person.ethnicity = "ethnicity3";
            } else if (ethnicity_name == "4") {
                person.ethnicity = "ethnicity4";
            } else {
                person.ethnicity = ethnicity_name; // Use the name as-is for other values
            }
            return;
        }
    }

    // If we reach here, no ethnicity was assigned - this indicates an error in probability
    // distribution
    std::vector<std::string> ethnicity_names;
    std::vector<double> probs;
    for (const auto &[name, prob] : ethnicity_probs) {
        ethnicity_names.push_back(name);
        probs.push_back(prob);
    }

    throw core::HgpsException(fmt::format(
        "Failed to assign ethnicity: cumulative probabilities do not sum to 1.0. "
        "Age group: {}, Gender: {}, Region: {}, Ethnicities: {}, Probabilities: {}, Cumulative "
        "sum: {}",
        age_group.to_string(), (person.gender == core::Gender::male) ? "male" : "female",
        person.region, fmt::format("[{}]", fmt::join(ethnicity_names, ", ")),
        fmt::format("[{}]", fmt::join(probs, ", ")), cumulative_prob));

    if (ethnicity_count % 5000 == 0) {
        std::cout << "\nSuccessfully initialized ethnicity for " << ethnicity_count << " people";
    }
}

void DemographicModule::set_region_prevalence(
    const std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>>
        &region_data) {
    region_prevalence_ = region_data;

    // Print summary of loaded region data
    if (!region_data.empty()) {
        std::cout << "\n=== REGION DATA LOADED ===" << '\n';

        // Get unique region names from the first age entry
        auto first_age = region_data.begin();
        if (first_age != region_data.end()) {
            auto first_gender = first_age->second.begin();
            if (first_gender != first_age->second.end()) {
                std::vector<std::string> region_names;
                for (const auto &[region, _] : first_gender->second) {
                    region_names.push_back(region);
                }
                std::cout << "Regions found: " << fmt::format("[{}]", fmt::join(region_names, ", "))
                          << '\n';
            }
        }

        std::cout << "Age groups: " << region_data.size() << '\n';
        std::cout << "Sample age: " << region_data.begin()->first.to_string() << '\n';
        std::cout << "=========================" << '\n';
    }
}

void DemographicModule::set_ethnicity_prevalence(
    const std::map<core::Identifier,
                   std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>>
        &ethnicity_data) {
    ethnicity_prevalence_ = ethnicity_data;

    // Print summary of loaded ethnicity data
    if (!ethnicity_data.empty()) {
        std::cout << "\n=== ETHNICITY DATA LOADED ===" << '\n';

        // Get unique ethnicity names from the first entry
        auto first_age_group = ethnicity_data.begin();
        if (first_age_group != ethnicity_data.end()) {
            auto first_gender = first_age_group->second.begin();
            if (first_gender != first_age_group->second.end()) {
                auto first_region = first_gender->second.begin();
                if (first_region != first_gender->second.end()) {
                    std::vector<std::string> ethnicity_names;
                    for (const auto &[ethnicity, _] : first_region->second) {
                        ethnicity_names.push_back(ethnicity);
                    }
                    std::cout << "Ethnicities found: "
                              << fmt::format("[{}]", fmt::join(ethnicity_names, ", ")) << '\n';
                }
            }
        }

        std::cout << "Age groups: " << ethnicity_data.size() << '\n';
        std::cout << "Sample age group: " << ethnicity_data.begin()->first.to_string() << '\n';
        std::cout << "=============================" << '\n';
    }
}

std::unique_ptr<DemographicModule> build_population_module(Repository &repository,
                                                           const ModelInput &config) {
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

    // Create the DemographicModule
    auto demographic_module =
        std::make_unique<DemographicModule>(std::move(pop_data), std::move(life_table));

    // Set region and ethnicity data from repository
    const auto &region_data = repository.get_region_prevalence();
    if (!region_data.empty()) {
        demographic_module->set_region_prevalence(region_data);
        std::cout << "\nDEBUG: Region data set in DemographicModule (" << region_data.size() << " age groups)";
    } else {
        std::cout << "\nDEBUG: No region data available in repository (this is OK for India/PIF projects)";
    }

    const auto &ethnicity_data = repository.get_ethnicity_prevalence();
    if (!ethnicity_data.empty()) {
        demographic_module->set_ethnicity_prevalence(ethnicity_data);
        std::cout << "\nDEBUG: Ethnicity data set in DemographicModule (" << ethnicity_data.size() << " age groups)";
    } else {
        std::cout << "\nDEBUG: No ethnicity data available in repository (this is OK for India/PIF projects)";
    }

    return demographic_module;
}
} // namespace hgps
