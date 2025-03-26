#include "demographic.h"
#include "HealthGPS.Core/thread_util.h"
#include "converter.h"
#include "sync_message.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <mutex>
#include <numeric>

#include "analysis_module.h"
#include "static_linear_model.h"
#include <oneapi/tbb/parallel_for_each.h>

namespace { // anonymous namespace

/// @brief Defines the residual mortality synchronisation message
using ResidualMortalityMessage = hgps::SyncDataMessage<hgps::GenderTable<int, double>>;

} // anonymous namespace

namespace hgps {

DemographicModule::DemographicModule(
    std::map<int, std::map<int, PopulationRecord>> &&pop_data, LifeTable &&life_table,
    std::unordered_map<core::Income, LinearModelParams> &&income_models,
    std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> &&region_models,
    std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> &&ethnicity_models,
    double income_continuous_stddev, double physical_activity_stddev)
    : pop_data_{std::move(pop_data)}, life_table_{std::move(life_table)},
      income_models_{std::move(income_models)}, region_models_{std::move(region_models)},
      ethnicity_models_{std::move(ethnicity_models)},
      income_continuous_stddev_{income_continuous_stddev},
      physical_activity_stddev_{physical_activity_stddev},
      expected_{std::make_shared<
          UnorderedMap2d<core::Gender, core::Identifier, std::map<int, double>>>()},
      expected_trend_{std::make_shared<std::unordered_map<core::Identifier, double>>()},
      trend_steps_{std::make_shared<std::unordered_map<core::Identifier, int>>()} {
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

std::string DemographicModule::name() const noexcept { return name_; }

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
    auto result = std::map<int, DoubleGenderValue>{};
    try {
        const auto &time_data = pop_data_.at(time_year);
        auto total = 0.0f;
        for (const auto &[age, record] : time_data) {
            result.emplace(age, DoubleGenderValue(record.males, record.females));
            total += record.total();
        }

        // Normalize to get probabilities
        for (auto &[age, value] : result) {
            value.males /= total;
            value.females /= total;
        }

        // Ensure each age group has a minimum representation
        // This prevents zero counts that lead to zero statistics
        for (auto &[age, value] : result) {
            if (value.males < 0.001) {
                value.males = 0.001; // Ensure a minimum proportion
            }
            if (value.females < 0.001) {
                value.females = 0.001; // Ensure a minimum proportion
            }
        }

        // Re-normalize after applying minimums
        total = 0.0f;
        for (const auto &[age, value] : result) {
            total += static_cast<float>(value.males + value.females);
        }
        for (auto &[age, value] : result) {
            value.males /= total;
            value.females /= total;
        }
    } catch (const std::exception &) {
        // Nothing to do, return empty result
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

void DemographicModule::initialise_age_gender(RuntimeContext &context, Population &population,
                                              [[maybe_unused]] Random &random) {
    // Get age/gender distribution for current year
    auto age_gender_dist = get_age_gender_distribution(context.time_now());

    // If age_gender_dist is empty or only contains age 100, create a realistic distribution
    if (age_gender_dist.empty() ||
        (age_gender_dist.size() == 1 && age_gender_dist.begin()->first == 100) ||
        (population.size() < 200)) { // Add small population check to ensure full age range
        std::cout << "WARNING: Missing, invalid, or small age distribution. Creating a realistic "
                     "distribution."
                  << std::endl;

        // Create a realistic age distribution from 0 to 100
        age_gender_dist.clear();
        double total_weight = 0.0;

        // Create age distribution with a wider spread to ensure older ages are represented
        for (int age = 0; age <= 100; age++) {
            double weight;
            if (age < 18) {
                weight = 0.2; // Children
            } else if (age < 65) {
                weight = 0.6; // Working age
            } else {
                weight = 0.3 * std::exp(-0.03 * (age - 65)); // Elderly with exponential decline
            }

            // Ensure minimum weight to guarantee some people in all age groups
            if (population.size() < 100 && age % 10 == 0) {
                weight = std::max(
                    weight, 0.05); // Force minimum weight for decade ages in small populations
            }

            total_weight += weight;
            age_gender_dist[age] =
                DoubleGenderValue(weight / 2.0, weight / 2.0); // Equal gender distribution
        }

        // Normalize weights
        for (auto &[age, value] : age_gender_dist) {
            value.males /= total_weight;
            value.females /= total_weight;
        }

        std::cout << "INFO: Created synthetic age distribution with realistic age curve"
                  << std::endl;
    }

    auto pop_size = static_cast<int>(population.size());
    auto entry_total = static_cast<int>(age_gender_dist.size());

    // Debug information
    std::cout << "Age distribution contains " << entry_total << " age groups" << std::endl;
    if (entry_total > 0) {
        std::cout << "Age range: " << age_gender_dist.begin()->first << " to "
                  << age_gender_dist.rbegin()->first << std::endl;

        // Debug: Print first few age distribution entries to verify
        int count = 0;
        for (const auto &[age, value] : age_gender_dist) {
            if (count < 5) {
                std::cout << "DEBUG: Age " << age << " - Males: " << value.males
                          << ", Females: " << value.females << std::endl;
                count++;
            } else {
                break;
            }
        }
    }

    // ONLY FOR TESTING WHEN USING A SMALL POPULATION
    // For small populations, ensure at least one person in each decade
    if (pop_size < 100) {
        std::cout << "Small population detected, ensuring representation across age groups"
                  << std::endl;

        // Create a distribution of ages to explicitly include
        std::vector<int> required_ages = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
        int reserved_count =
            required_ages.size() * 2; // One male and one female for each required age

        // First, assign the required ages
        int index = 0;
        for (auto age : required_ages) {
            if (index < pop_size) {
                population[index].age = age;
                population[index].gender = core::Gender::male;
                index++;
            }

            if (index < pop_size) {
                population[index].age = age;
                population[index].gender = core::Gender::female;
                index++;
            }
        }

        // Then, distribute the remaining population according to the distribution
        if (index < pop_size) {
            auto remaining = pop_size - index;

            // Initialize the index counter to track position in the population array
            for (auto entry_count = 1; auto &entry : age_gender_dist) {
                auto age = entry.first; // Store the age value explicitly
                auto num_males = static_cast<int>(std::round(remaining * entry.second.males));
                auto num_females = static_cast<int>(std::round(remaining * entry.second.females));

                // Adjust for small values
                if (entry.second.males > 0 && num_males == 0)
                    num_males = 1;
                if (entry.second.females > 0 && num_females == 0)
                    num_females = 1;

                // Check if we're going to exceed the array bounds
                if (index + num_males + num_females > pop_size) {
                    num_males = (pop_size - index) / 2;
                    num_females = (pop_size - index) - num_males;
                }

                // Assign males
                for (auto i = 0; i < num_males && index < pop_size; i++) {
                    population[index].age = age;
                    population[index].gender = core::Gender::male;
                    index++;
                }

                // Assign females
                for (auto i = 0; i < num_females && index < pop_size; i++) {
                    population[index].age = age;
                    population[index].gender = core::Gender::female;
                    index++;
                }

                // If we've filled the population, we're done
                if (index >= pop_size)
                    break;

                entry_count++;
            }
        }
    } else {
        // Original distribution logic for larger populations
        // Initialize the index counter to track position in the population array
        int index = 0;

        for (auto entry_count = 1; auto &entry : age_gender_dist) {
            auto age = entry.first; // Store the age value explicitly
            auto num_males = static_cast<int>(std::round(pop_size * entry.second.males));
            auto num_females = static_cast<int>(std::round(pop_size * entry.second.females));

            // Ensure at least one person per age group if probability is non-zero
            if (entry.second.males > 0 && num_males == 0)
                num_males = 1;
            if (entry.second.females > 0 && num_females == 0)
                num_females = 1;

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
                population[index].age = age;
                population[index].gender = core::Gender::male;
                index++;
            }

            // [index + num_males, num_required)
            for (auto i = 0; i < num_females; i++) {
                population[index].age = age;
                population[index].gender = core::Gender::female;
                index++;
            }

            entry_count++;
        }

        assert(index == pop_size);
    }

    // Print age distribution summary
    std::map<int, int> age_count;
    for (const auto &person : population) {
        age_count[person.age]++;
    }

    // Check if we have representation in each age group
    std::cout << "Final age distribution:" << std::endl;
    std::cout << "  Age 0-20: "
              << std::accumulate(age_count.begin(),
                                 std::upper_bound(age_count.begin(), age_count.end(), 20,
                                                  [](int v, const auto &p) { return v < p.first; }),
                                 0, [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
    std::cout << "  Age 21-40: "
              << std::accumulate(std::lower_bound(age_count.begin(), age_count.end(), 21,
                                                  [](const auto &p, int v) { return p.first < v; }),
                                 std::upper_bound(age_count.begin(), age_count.end(), 40,
                                                  [](int v, const auto &p) { return v < p.first; }),
                                 0, [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
    std::cout << "  Age 41-60: "
              << std::accumulate(std::lower_bound(age_count.begin(), age_count.end(), 41,
                                                  [](const auto &p, int v) { return p.first < v; }),
                                 std::upper_bound(age_count.begin(), age_count.end(), 60,
                                                  [](int v, const auto &p) { return v < p.first; }),
                                 0, [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
    std::cout << "  Age 61-80: "
              << std::accumulate(std::lower_bound(age_count.begin(), age_count.end(), 61,
                                                  [](const auto &p, int v) { return p.first < v; }),
                                 std::upper_bound(age_count.begin(), age_count.end(), 80,
                                                  [](int v, const auto &p) { return v < p.first; }),
                                 0, [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
    std::cout << "  Age 81+: "
              << std::accumulate(std::lower_bound(age_count.begin(), age_count.end(), 81,
                                                  [](const auto &p, int v) { return p.first < v; }),
                                 age_count.end(), 0,
                                 [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
}

// Modified and updated- Mahima
// This initializes the enrire virtual population with age, gender, region (set to 1), ethnicity,
// income_continuous, income_category and physical activity
void DemographicModule::initialise_population(RuntimeContext &context, Population &population,
                                              Random &random) {
    std::cout << "DEBUG: Initializing population of size " << population.size() << std::endl;

    // First, ensure all persons in the population are active
    // This is critical to fix any issues from previous runs
    for (std::size_t i = 0; i < population.size(); i++) {
        // We need to create new person objects to ensure they start with active status
        auto gender = population[i].gender;
        population[i] = Person(gender);
    }

    // Step 1: Initialize age and gender distribution
    initialise_age_gender(context, population, random);

    // Debug: Print age distribution statistics after initialization
    std::map<int, int> age_groups;
    for (const auto &person : population) {
        if (person.is_active()) {
            age_groups[person.age]++;
        }
    }

    // Print summary
    std::cout << "DEBUG: Population age distribution after initialization:" << std::endl;
    std::cout << "  Age 0-20: "
              << std::accumulate(age_groups.begin(),
                                 std::upper_bound(age_groups.begin(), age_groups.end(), 20,
                                                  [](int v, const auto &p) { return v < p.first; }),
                                 0, [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
    std::cout << "  Age 21-40: "
              << std::accumulate(std::lower_bound(age_groups.begin(), age_groups.end(), 21,
                                                  [](const auto &p, int v) { return p.first < v; }),
                                 std::upper_bound(age_groups.begin(), age_groups.end(), 40,
                                                  [](int v, const auto &p) { return v < p.first; }),
                                 0, [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
    std::cout << "  Age 41-60: "
              << std::accumulate(std::lower_bound(age_groups.begin(), age_groups.end(), 41,
                                                  [](const auto &p, int v) { return p.first < v; }),
                                 std::upper_bound(age_groups.begin(), age_groups.end(), 60,
                                                  [](int v, const auto &p) { return v < p.first; }),
                                 0, [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
    std::cout << "  Age 61-80: "
              << std::accumulate(std::lower_bound(age_groups.begin(), age_groups.end(), 61,
                                                  [](const auto &p, int v) { return p.first < v; }),
                                 std::upper_bound(age_groups.begin(), age_groups.end(), 80,
                                                  [](int v, const auto &p) { return v < p.first; }),
                                 0, [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;
    std::cout << "  Age 81+: "
              << std::accumulate(std::lower_bound(age_groups.begin(), age_groups.end(), 81,
                                                  [](const auto &p, int v) { return p.first < v; }),
                                 age_groups.end(), 0,
                                 [](int sum, const auto &p) { return sum + p.second; })
              << " people" << std::endl;

    // Step 2: Initialize region and ethnicity for each person
    for (auto &person : population) {
        initialise_region(context, person, random);
        initialise_ethnicity(context, person, random);
    }

    // Step 3: Initialize income for active persons
    for (auto &person : population) {
        if (person.is_active()) {
            initialise_income_continuous(person, random);
        }
    }

    // Step 4: Calculate income thresholds and initialize income categories
    auto [q1_threshold, q2_threshold, q3_threshold] = calculate_income_thresholds(population);

    // Debug income category counts
    int low_count = 0, lower_middle_count = 0, upper_middle_count = 0, high_count = 0;

    for (auto &person : population) {
        if (person.is_active()) {
            initialise_income_category(person, q1_threshold, q2_threshold, q3_threshold);

            // Count categories for debugging
            switch (person.income_category) {
            case core::Income::low:
                low_count++;
                break;
            case core::Income::lowermiddle:
                lower_middle_count++;
                break;
            case core::Income::uppermiddle:
                upper_middle_count++;
                break;
            case core::Income::high:
                high_count++;
                break;
            }
        }
    }

    // Print income category distribution
    std::cout << "DEBUG: Income category distribution:" << std::endl;
    std::cout << "  Low: " << low_count << " people" << std::endl;
    std::cout << "  Lower Middle: " << lower_middle_count << " people" << std::endl;
    std::cout << "  Upper Middle: " << upper_middle_count << " people" << std::endl;
    std::cout << "  High: " << high_count << " people" << std::endl;

    // Print some example individuals with their income and category
    std::cout << "DEBUG: Income category examples:" << std::endl;
    int examples_shown = 0;
    for (const auto &person : population) {
        if (person.is_active() && examples_shown < 10) {
            std::string category;
            switch (person.income_category) {
            case core::Income::low:
                category = "Low";
                break;
            case core::Income::lowermiddle:
                category = "Lower Middle";
                break;
            case core::Income::uppermiddle:
                category = "Upper Middle";
                break;
            case core::Income::high:
                category = "High";
                break;
            default:
                category = "Unknown";
            }

            std::cout << "  Person " << examples_shown << " - Age: " << person.age
                      << ", Income: " << person.income_continuous << ", Category: " << category
                      << std::endl;
            examples_shown++;
        }
    }

    // Step 5: Initialize physical activity for each person
    for (auto &person : population) {
        initialise_physical_activity(context, person, random);
    }
}

void DemographicModule::update_population(RuntimeContext &context,
                                          const DiseaseModule &disease_host) {
    std::cout << "DEBUG: Starting update_population, active size: "
              << context.population().current_active_size() << std::endl;

    // Start the residual mortality calculation asynchronously
    auto residual_future = core::run_async(&DemographicModule::update_residual_mortality, this,
                                           std::ref(context), std::ref(disease_host));

    auto initial_pop_size = context.population().current_active_size();

    // ENHANCED CHECK: If we have no active population, reset it immediately
    if (initial_pop_size == 0 && context.population().size() > 0) {
        std::cout << "CRITICAL WARNING: No active population detected at start of update cycle. "
                     "Performing immediate reinitialize..."
                  << std::endl;

        // Get expected population size for current time
        auto expected_pop_size = get_total_population_size(context.time_now());
        float size_fraction = context.inputs().settings().size_fraction();
        int target_size = static_cast<int>(expected_pop_size * size_fraction);

        // Reset and re-initialize population
        context.reset_population(target_size);
        initialise_population(context, context.population(), context.random());

        // Update initial population size after reinitialization
        initial_pop_size = context.population().current_active_size();
        std::cout << "RECOVERY: Population reinitialized with " << initial_pop_size
                  << " active members." << std::endl;
    }

    auto expected_pop_size = get_total_population_size(context.time_now());
    auto expected_num_deaths = get_total_deaths(context.time_now());

    std::cout << "DEBUG: Before residual_future.get(), active size: "
              << context.population().current_active_size() << std::endl;

    // Wait for the residual mortality calculation with timeout
    bool residual_success = false;
    try {
        // Wait for the residual_future with a reasonable timeout (60 seconds)
        if (residual_future.wait_for(std::chrono::seconds(60)) == std::future_status::ready) {
            // Safely get the result
            try {
                residual_future.get(); // No need to store the result
                residual_success = true;
                std::cout << "DEBUG: Residual mortality calculation completed successfully"
                          << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "ERROR: Exception during residual mortality calculation: " << e.what()
                          << std::endl;
            } catch (...) {
                std::cerr << "ERROR: Unknown exception during residual mortality calculation"
                          << std::endl;
            }
        } else {
            std::cerr << "ERROR: Residual mortality calculation timed out after 60 seconds"
                      << std::endl;
            // Let the future continue in background, but we'll proceed without waiting for it
        }
    } catch (const std::exception &e) {
        std::cerr << "ERROR: Exception while waiting for residual mortality: " << e.what()
                  << std::endl;
    } catch (...) {
        std::cerr << "ERROR: Unknown exception while waiting for residual mortality" << std::endl;
    }

    // If residual mortality calculation failed, initialize with default values
    if (!residual_success) {
        std::cout << "WARNING: Using default residual mortality values due to calculation failure"
                  << std::endl;

        // Create a table that covers ages 0-110 if it's empty
        if (residual_death_rates_.empty()) {
            residual_death_rates_ =
                create_integer_gender_table<double>(core::IntegerInterval(0, 110));
        }

        // Set default values that will allow simulation to continue
        for (int age = 0; age <= 110; age++) {
            residual_death_rates_.at(age, core::Gender::male) =
                0.001 + (age / 1000.0); // Small age-dependent rate
            residual_death_rates_.at(age, core::Gender::female) =
                0.001 + (age / 1200.0); // Slightly lower for females
        }
    }

    // ENHANCED CHECK: Population might go inactive during residual_future processing
    if (context.population().current_active_size() == 0 && initial_pop_size > 0) {
        std::cout << "CRITICAL WARNING: Population became inactive during residual mortality "
                     "calculation. Restoring..."
                  << std::endl;

        // Reset and re-initialize population
        context.reset_population(initial_pop_size);
        initialise_population(context, context.population(), context.random());

        std::cout << "RECOVERY: Population restored with "
                  << context.population().current_active_size() << " active members." << std::endl;
    }

    std::cout << "DEBUG: After residual_future.get(), active size: "
              << context.population().current_active_size() << std::endl;

    auto number_of_deaths = update_age_and_death_events(context, disease_host);

    std::cout << "DEBUG: After update_age_and_death_events, active size: "
              << context.population().current_active_size() << std::endl;

    // apply births events
    auto last_year_births_rate = get_birth_rate(context.time_now() - 1);
    auto number_of_boys = static_cast<int>(last_year_births_rate.males * initial_pop_size);
    auto number_of_girls = static_cast<int>(last_year_births_rate.females * initial_pop_size);

    std::cout << "DEBUG: Before adding newborns, active size: "
              << context.population().current_active_size() << std::endl;

    context.population().add_newborn_babies(number_of_boys, core::Gender::male, context.time_now());
    context.population().add_newborn_babies(number_of_girls, core::Gender::female,
                                            context.time_now());

    std::cout << "DEBUG: After adding newborns, active size: "
              << context.population().current_active_size() << std::endl;

    // Calculate statistics.
    if (initial_pop_size <= 0) {
        initial_pop_size = 1; // Use 1 as the minimum denominator
    }

    if (expected_pop_size <= 0) {
        expected_pop_size = 1; // Use 1 as the minimum denominator
    }

    auto simulated_death_rate = number_of_deaths * 1000.0 / initial_pop_size;
    auto expected_death_rate = expected_num_deaths * 1000.0 / expected_pop_size;

    // Final sanity check to ensure rates are valid
    if (!std::isfinite(simulated_death_rate)) {
        simulated_death_rate = 0.0;
    }
    if (!std::isfinite(expected_death_rate)) {
        expected_death_rate = 0.0;
    }

    // Similarly, for any divide by zero situations that might occur when calculating percentages:
    double death_rate_delta_percent = 0.0;
    if (expected_death_rate > 0.0) {
        death_rate_delta_percent = 100 * (simulated_death_rate / expected_death_rate - 1);
        // Check result is valid
        if (!std::isfinite(death_rate_delta_percent)) {
            death_rate_delta_percent = 0.0;
        }
    }

    // Then update metrics:
    context.metrics()["SimulatedDeathRate"] = simulated_death_rate;
    context.metrics()["ExpectedDeathRate"] = expected_death_rate;
    context.metrics()["DeathRateDeltaPercent"] = death_rate_delta_percent;

    // Final check - if population is still zero after all operations, we need to reset
    if (context.population().current_active_size() == 0 && initial_pop_size > 0) {
        std::cout << "CRITICAL WARNING: Population is empty after demographic updates. Final "
                     "attempt to restore..."
                  << std::endl;

        // Get expected population size for current time
        auto expected_pop_size = get_total_population_size(context.time_now());
        float size_fraction = context.inputs().settings().size_fraction();
        int target_size = static_cast<int>(expected_pop_size * size_fraction);

        // Reset and re-initialize population
        context.reset_population(target_size);
        initialise_population(context, context.population(), context.random());

        std::cout << "FINAL RECOVERY: Population restored with "
                  << context.population().current_active_size() << " active members." << std::endl;
    }

    std::cout << "DEBUG: Ending update_population, active size: "
              << context.population().current_active_size() << std::endl;
}

void DemographicModule::update_residual_mortality(RuntimeContext &context,
                                                  const DiseaseModule &disease_host) {
    try {
        std::cout << "DEBUG: [update_residual_mortality] Starting calculation..." << std::endl;
        auto excess_mortality_product =
            create_integer_gender_table<double>(life_table_.age_limits());
        auto excess_mortality_count = create_integer_gender_table<int>(life_table_.age_limits());
        auto &pop = context.population();
        auto sum_mutex = std::mutex{};

        // First check if we have any active population
        size_t active_count = 0;
        for (const auto &entity : pop) {
            if (entity.is_active()) {
                active_count++;
                if (active_count >= 10)
                    break; // Stop counting after finding enough people
            }
        }

        if (active_count < 10) {
            std::cerr << "WARNING: Very small active population (" << active_count
                      << ") for residual mortality calculation" << std::endl;
            // Continue with calculation but be cautious
        }

        // Process each person with proper exception handling
        std::vector<std::string> errors;
        tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
            try {
                if (!entity.is_active()) {
                    return;
                }

                if (entity.age < 0 || entity.age > 110) {
                    std::string err = "Invalid age " + std::to_string(entity.age) + " for person " +
                                      std::to_string(entity.id());
                    auto lock = std::unique_lock{sum_mutex};
                    errors.push_back(err);
                    return;
                }

                // Calculate the excess mortality product with error checking
                double product = 1.0;
                try {
                    product = calculate_excess_mortality_product(entity, disease_host);
                } catch (const std::exception &e) {
                    auto lock = std::unique_lock{sum_mutex};
                    errors.push_back(std::string("Error calculating excess mortality: ") +
                                     e.what());
                    product = 1.0; // Default value
                }

                // Safely update the tables
                auto lock = std::unique_lock{sum_mutex};
                excess_mortality_product.at(entity.age, entity.gender) += product;
                excess_mortality_count.at(entity.age, entity.gender)++;
            } catch (const std::exception &e) {
                auto lock = std::unique_lock{sum_mutex};
                errors.push_back(std::string("Error processing person: ") + e.what());
            }
        });

        // Report any errors (limit to first 10)
        if (!errors.empty()) {
            std::cerr << "WARNING: " << errors.size()
                      << " errors during residual mortality calculation" << std::endl;
            for (size_t i = 0; i < std::min(errors.size(), static_cast<size_t>(10)); i++) {
                std::cerr << "  Error " << (i + 1) << ": " << errors[i] << std::endl;
            }
            if (errors.size() > 10) {
                std::cerr << "  (and " << (errors.size() - 10) << " more errors)" << std::endl;
            }
        }

        auto death_rates = create_death_rates_table(context.time_now());
        auto residual_mortality = calculate_residual_mortality(context, disease_host);

        // Store the calculated mortality in the residual_death_rates_ member
        auto start_age = life_table_.age_limits().lower();
        auto end_age = life_table_.age_limits().upper();
        for (int age = start_age; age <= end_age; age++) {
            if (residual_mortality.contains(age)) {
                residual_death_rates_.at(age, core::Gender::male) =
                    residual_mortality.at(age, core::Gender::male);
                residual_death_rates_.at(age, core::Gender::female) =
                    residual_mortality.at(age, core::Gender::female);
            }
        }

        std::cout << "DEBUG: [update_residual_mortality] Calculation completed successfully"
                  << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "ERROR in update_residual_mortality: " << e.what() << std::endl;

        // Create default values
        auto start_age = life_table_.age_limits().lower();
        auto end_age = life_table_.age_limits().upper();

        for (int age = start_age; age <= end_age; age++) {
            residual_death_rates_.at(age, core::Gender::male) = 0.001 + (age / 1000.0);
            residual_death_rates_.at(age, core::Gender::female) = 0.001 + (age / 1200.0);
        }
    } catch (...) {
        std::cerr << "UNKNOWN ERROR in update_residual_mortality" << std::endl;

        // Create default values
        auto start_age = life_table_.age_limits().lower();
        auto end_age = life_table_.age_limits().upper();

        for (int age = start_age; age <= end_age; age++) {
            residual_death_rates_.at(age, core::Gender::male) = 0.001 + (age / 1000.0);
            residual_death_rates_.at(age, core::Gender::female) = 0.001 + (age / 1200.0);
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
    std::cout << "DEBUG: Maximum age limit is " << max_age << std::endl;

    auto number_of_deaths = 0;
    auto age_deaths = 0;
    auto probability_deaths = 0;

    // Count active population ages before updates
    std::map<int, int> age_counts;
    for (const auto &entity : context.population()) {
        if (entity.is_active()) {
            age_counts[entity.age]++;
        }
    }

    // Print age distribution summary
    std::cout << "DEBUG: Age distribution before update:" << std::endl;
    std::cout << "  Age 0-20: "
              << std::accumulate(age_counts.begin(), age_counts.upper_bound(20), 0,
                                 [](int sum, const auto &pair) { return sum + pair.second; })
              << " people" << std::endl;
    std::cout << "  Age 21-40: "
              << std::accumulate(age_counts.lower_bound(21), age_counts.upper_bound(40), 0,
                                 [](int sum, const auto &pair) { return sum + pair.second; })
              << " people" << std::endl;
    std::cout << "  Age 41-60: "
              << std::accumulate(age_counts.lower_bound(41), age_counts.upper_bound(60), 0,
                                 [](int sum, const auto &pair) { return sum + pair.second; })
              << " people" << std::endl;
    std::cout << "  Age 61-80: "
              << std::accumulate(age_counts.lower_bound(61), age_counts.upper_bound(80), 0,
                                 [](int sum, const auto &pair) { return sum + pair.second; })
              << " people" << std::endl;
    std::cout << "  Age 81+: "
              << std::accumulate(age_counts.lower_bound(81), age_counts.end(), 0,
                                 [](int sum, const auto &pair) { return sum + pair.second; })
              << " people" << std::endl;

    // For debugging death probabilities - sample a few individuals
    std::cout << "DEBUG: Death probability samples:" << std::endl;
    int samples_shown = 0;

    for (auto &entity : context.population()) {
        if (!entity.is_active()) {
            continue;
        }

        bool will_die = false;

        if (entity.age >= max_age) {
            will_die = true;
            entity.die(context.time_now());
            number_of_deaths++;
            age_deaths++;

            // Debug first few deaths
            if (samples_shown < 3) {
                std::cout << "  Person died (max age): Age " << entity.age << ", Gender: "
                          << (entity.gender == core::Gender::male ? "Male" : "Female") << std::endl;
                samples_shown++;
            }
        } else {
            // Calculate death probability based on the health status
            double residual_death_rate = get_residual_death_rate(entity.age, entity.gender);

            // Ensure residual_death_rate is valid and not excessive
            if (std::isnan(residual_death_rate) || !std::isfinite(residual_death_rate)) {
                std::cout << "WARNING: Invalid residual death rate for Age " << entity.age
                          << ", Gender: "
                          << (entity.gender == core::Gender::male ? "Male" : "Female")
                          << ". Using default value of 0.001" << std::endl;
                residual_death_rate = 0.001; // Safe default value
            }

            // Cap excessively high death rates to prevent everyone from dying
            if (residual_death_rate > 0.5) {
                std::cout << "WARNING: Unusually high residual death rate " << residual_death_rate
                          << " for Age " << entity.age << ", Gender: "
                          << (entity.gender == core::Gender::male ? "Male" : "Female")
                          << ". Capping at 0.5" << std::endl;
                residual_death_rate = 0.5;
            }

            auto product = 1.0 - residual_death_rate;
            for (const auto &item : entity.diseases) {
                if (item.second.status == DiseaseStatus::active) {
                    auto excess_mortality = disease_host.get_excess_mortality(item.first, entity);
                    product *= (1.0 - excess_mortality);
                }
            }

            auto death_probability = 1.0 - product;

            // Sanity check the death probability
            if (death_probability < 0.0 || death_probability > 1.0 ||
                std::isnan(death_probability) || !std::isfinite(death_probability)) {
                std::cout << "WARNING: Invalid death probability " << death_probability
                          << " for Age " << entity.age << ", Gender: "
                          << (entity.gender == core::Gender::male ? "Male" : "Female")
                          << ". Using default value of 0.001" << std::endl;
                death_probability = 0.001;
            }

            auto hazard = context.random().next_double();

            // Debug first few people regardless of outcome
            if (samples_shown < 5) {
                std::cout << "  Person check: Age " << entity.age << ", Gender: "
                          << (entity.gender == core::Gender::male ? "Male" : "Female")
                          << ", Death prob: " << death_probability << ", Hazard: " << hazard
                          << ", Residual rate: " << residual_death_rate << std::endl;
                samples_shown++;
            }

            if (hazard < death_probability) {
                will_die = true;
                entity.die(context.time_now());
                number_of_deaths++;
                probability_deaths++;
            }
        }

        if (entity.is_active()) {
            // Verify the entity is still active after potential death events
            if (will_die) {
                std::cout << "WARNING: Person marked to die but still active!" << std::endl;
            }
            entity.age = entity.age + 1;
        }
    }

    // Count active population after update for comparison
    std::map<int, int> post_age_counts;
    int active_after = 0;
    for (const auto &entity : context.population()) {
        if (entity.is_active()) {
            post_age_counts[entity.age]++;
            active_after++;
        }
    }

    std::cout << "DEBUG: Deaths summary - Total: " << number_of_deaths
              << ", Age-related: " << age_deaths << ", Probability-based: " << probability_deaths
              << ", Active before: " << context.population().current_active_size()
              << ", Active after: " << active_after << std::endl;

    return number_of_deaths;
}

void DemographicModule::initialise_region(RuntimeContext &context, Person &person, Random &random) {
    // Get probabilities for this age/sex stratum
    auto region_probs = context.inputs().get_region_probabilities(person.age, person.gender);

    // If probabilities are empty, use the region_models loaded from static_model.json
    if (region_probs.empty() && region_models_) {
        // Use the region models loaded from static_model.json
        double total = 0.0;
        for (const auto &[region, model] : *region_models_) {
            // Simple sum of intercepts for probability distribution
            region_probs[region] = model.intercept;
            total += model.intercept;
        }

        // Handle special case where only one region has non-zero probability
        bool single_region = false;
        core::Region only_region = core::Region::England; // Default

        if (total == 0.0) {
            // If all intercepts are zero, use England as default
            region_probs[core::Region::England] = 1.0;
            single_region = true;
            only_region = core::Region::England;
        } else if (region_probs.size() == 1 ||
                   (region_probs.contains(core::Region::England) &&
                    std::abs(region_probs[core::Region::England] - 1.0) < 0.001 &&
                    region_probs.size() > 1)) {
            // If only England has probability 1 (or very close to 1)
            single_region = true;
            only_region = core::Region::England;
            region_probs[core::Region::England] = 1.0;

            // Reset other regions to zero
            for (auto &[region, prob] : region_probs) {
                if (region != core::Region::England) {
                    prob = 0.0;
                }
            }
        } else {
            // Normal case - normalize probabilities
            if (total > 0.0) {
                for (auto &[region, prob] : region_probs) {
                    prob /= total;
                }
            }
        }

        // If we have a single region with probability 1, just assign it directly
        if (single_region) {
            person.region = only_region;
            return;
        }
    }

    // Use CDF for assignment
    double rand_value = random.next_double(); // next_double is always between 0,1
    double cumulative_prob = 0.0;

    for (const auto &[region, prob] : region_probs) {
        cumulative_prob += prob;
        if (rand_value < cumulative_prob) { // Changed from <= to < for correct CDF sampling
            person.region = region;
            return;
        }
    }

    // Fallback: assign England if assignment fails
    person.region = core::Region::England;
}

void DemographicModule::initialise_ethnicity(RuntimeContext &context, Person &person,
                                             Random &random) {
    // Get ethnicity probabilities directly from input data based on age/gender
    const bool is_under_18 = person.age < 18;
    const std::string age_group = is_under_18 ? "Under18" : "Over18";

    // Get ethnicity probabilities for this age/gender combination
    auto ethnicity_probs = std::unordered_map<core::Ethnicity, double>();

    // Directly access ethnicity prevalence data
    try {
        // Get prevalence data from the repository/data manager
        auto prevalence_data =
            context.inputs().get_ethnicity_probabilities(person.age, person.gender, person.region);

        if (prevalence_data.empty()) {
            // Use default distribution if no data available
            ethnicity_probs[core::Ethnicity::White] = 0.75;
            ethnicity_probs[core::Ethnicity::Asian] = 0.15;
            ethnicity_probs[core::Ethnicity::Black] = 0.05;
            ethnicity_probs[core::Ethnicity::Others] = 0.05;
        } else {
            // Use the prevalence data directly
            ethnicity_probs = prevalence_data;
        }
    } catch (const std::exception &) {
        // Fall back to default distribution
        ethnicity_probs[core::Ethnicity::White] = 0.75;
        ethnicity_probs[core::Ethnicity::Asian] = 0.15;
        ethnicity_probs[core::Ethnicity::Black] = 0.05;
        ethnicity_probs[core::Ethnicity::Others] = 0.05;
    }

    // Validate the probabilities sum to 1.0 (with small tolerance for floating point errors)
    double total = 0.0;
    for (const auto &[ethnicity, prob] : ethnicity_probs) {
        total += prob;
    }

    if (std::abs(total - 1.0) > 0.001) {
        // Normalize probabilities to ensure they sum to 1.0
        for (auto &[ethnicity, prob] : ethnicity_probs) {
            prob /= total;
        }
    }

    // Use CDF for assignment
    double rand_value = random.next_double(); // next_double is between 0,1
    double cumulative_prob = 0.0;

    for (const auto &[ethnicity, prob] : ethnicity_probs) {
        cumulative_prob += prob;
        if (rand_value < cumulative_prob) {
            person.ethnicity = ethnicity;
            return;
        }
    }

    // Find the ethnicity with highest probability and assign it as fallback
    core::Ethnicity highest_prob_ethnicity = core::Ethnicity::White; // Default
    double highest_prob = 0.0;

    for (const auto &[ethnicity, prob] : ethnicity_probs) {
        if (prob > highest_prob) {
            highest_prob = prob;
            highest_prob_ethnicity = ethnicity;
        }
    }

    person.ethnicity = highest_prob_ethnicity;
}

void DemographicModule::initialise_income_continuous(Person &person, Random &random) {
    // Initialize base income value - Start with intercept
    double income_base = 0.0;

    try {
        // Add intercept from the income model if available
        if (!income_models_.empty()) {
            // Start with the intercept value
            income_base = income_models_.begin()->second.intercept;
        }

        // Add age - apply coefficient to actual age using one model
        if (!income_models_.empty() &&
            income_models_.begin()->second.coefficients.count("Age") > 0) {
            income_base += income_models_.begin()->second.coefficients.at("Age") * person.age;
        }

        // Add gender - apply coefficient to binary gender value using one model
        double gender_value = (person.gender == core::Gender::female) ? 1.0 : 0.0;
        if (!income_models_.empty() &&
            income_models_.begin()->second.coefficients.count("Gender") > 0) {
            income_base += income_models_.begin()->second.coefficients.at("Gender") * gender_value;
        }

        // Add region - apply coefficient to region value, safely checking if the region exists in
        // the map
        double region_value = person.region_to_value();
        if (region_models_ && region_models_->count(person.region) > 0) {
            const auto &region_model = region_models_->at(person.region);
            if (region_model.coefficients.count("Income") > 0) {
                income_base += region_model.coefficients.at("Income") * region_value;
            }
        }

        // Add ethnicity - apply coefficient to ethnicity value, safely checking if the ethnicity
        // exists in the map
        double ethnicity_value = person.ethnicity_to_value();
        if (ethnicity_models_ && ethnicity_models_->count(person.ethnicity) > 0) {
            const auto &ethnicity_model = ethnicity_models_->at(person.ethnicity);
            if (ethnicity_model.coefficients.count("Income") > 0) {
                income_base += ethnicity_model.coefficients.at("Income") * ethnicity_value;
            }
        }

        // Add random variation with specified standard deviation
        double rand = random.next_normal(0.0, income_continuous_stddev_);

        // Ensure income is positive and apply the random variation
        person.income_continuous = std::max(0.1, income_base * (1.0 + rand));

    } catch (const std::exception &) {
        // Fallback to a reasonable default value
        person.income_continuous = 1.0 + random.next_normal(0.0, 0.2);
    }
}

void DemographicModule::initialise_income_category(Person &person, double q1_threshold,
                                                   double q2_threshold, double q3_threshold) {
    // Debug info for income category assignment
    bool debug_income = false;

    if (debug_income) {
        std::cout << "Income thresholds: Q1=" << q1_threshold << ", Q2=" << q2_threshold
                  << ", Q3=" << q3_threshold << ", Person income=" << person.income_continuous
                  << std::endl;
    }

    // Assign income categories based on quartiles - using < instead of <= for proper quartile
    // division
    if (person.income_continuous < q1_threshold) {
        person.income_category = core::Income::low;
    } else if (person.income_continuous < q2_threshold) {
        person.income_category = core::Income::lowermiddle;
    } else if (person.income_continuous < q3_threshold) {
        person.income_category = core::Income::uppermiddle;
    } else {
        person.income_category = core::Income::high;
    }
}

std::tuple<double, double, double>
DemographicModule::calculate_income_thresholds(const Population &population) {
    std::vector<double> sorted_incomes;
    sorted_incomes.reserve(population.size());

    // Collect all income values from active population members
    for (const auto &p : population) {
        if (p.is_active() && p.income_continuous >= 0) {
            sorted_incomes.push_back(p.income_continuous);
        }
    }

    // Sort once
    std::sort(sorted_incomes.begin(), sorted_incomes.end());

    // Calculate quartile thresholds
    size_t n = sorted_incomes.size();
    if (n == 0) {
        return {0.0, 0.0, 0.0}; // Handle empty population case
    }

    // Use correct quartile calculations
    // For a proper quartile division, we want:
    // Q1 = 25th percentile, Q2 = 50th percentile (median), Q3 = 75th percentile
    double q1_threshold = sorted_incomes[n / 4];
    double q2_threshold = sorted_incomes[n / 2];
    double q3_threshold = sorted_incomes[(3 * n) / 4];

    return {q1_threshold, q2_threshold, q3_threshold};
}

void DemographicModule::initialise_physical_activity(RuntimeContext &context, Person &person,
                                                     Random &random) {
    try {
        // Start with the intercept from the PhysicalActivityModels if available
        double base_pa = 0.0;

        // Find the physical activity model (assuming it's in one of the income models)
        for (const auto &[income_level, model] : income_models_) {
            if (model.coefficients.count("PhysicalActivity") > 0) {
                // We found a model containing physical activity coefficients
                // Use its intercept as a starting point
                base_pa = model.intercept;
                break;
            }
        }

        // If no intercept found yet, try to use the expected value
        if (base_pa == 0.0) {
            // Calculate base expected PA value for age and gender
            base_pa = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                   std::nullopt, false);
        }

        // Apply modifiers based on region, if available
        double region_effect = 0.0;
        if (region_models_ && region_models_->count(person.region) > 0) {
            const auto &region_params = region_models_->at(person.region);
            if (region_params.coefficients.count("PhysicalActivity"_id) > 0) {
                region_effect = region_params.coefficients.at("PhysicalActivity"_id);
                base_pa *= (1.0 + region_effect);
            }
        }

        // Apply modifiers based on ethnicity, if available
        double ethnicity_effect = 0.0;
        if (ethnicity_models_ && ethnicity_models_->count(person.ethnicity) > 0) {
            const auto &ethnicity_params = ethnicity_models_->at(person.ethnicity);
            if (ethnicity_params.coefficients.count("PhysicalActivity"_id) > 0) {
                ethnicity_effect = ethnicity_params.coefficients.at("PhysicalActivity"_id);
                base_pa *= (1.0 + ethnicity_effect);
            }
        }

        // Apply modifiers based on continuous income - using all income models
        double income_effect = 0.0;
        for (const auto &[income_level, model] : income_models_) {
            if (model.coefficients.count("PhysicalActivity") > 0) {
                income_effect +=
                    model.coefficients.at("PhysicalActivity") * person.income_continuous;
            }
        }
        base_pa *= (1.0 + income_effect);

        // Add random variation using physical_activity_stddev_
        double noise = random.next_normal(0.0, physical_activity_stddev_);
        base_pa *= (1.0 + noise);

        // Set the physical activity value (ensure it's within reasonable range)
        double pa_value = std::max(1.2, std::min(2.0, base_pa));
        person.risk_factors["PhysicalActivity"_id] = pa_value;
    } catch (const std::exception &) {
        // Use a fallback value if calculation fails
        person.risk_factors["PhysicalActivity"_id] = 1.5;
    }
}

double DemographicModule::get_expected([[maybe_unused]] RuntimeContext &context,
                                       core::Gender gender, int age, const core::Identifier &factor,
                                       std::optional<core::DoubleInterval> range,
                                       bool apply_trend) const noexcept {
    // Check if the expected_ pointer is valid and contains the required data
    if (!expected_ || !expected_->contains(gender, factor) ||
        !expected_->at(gender, factor).contains(age)) {
        // Return a minimum positive value instead of zero to ensure calculations don't fail
        return 0.01;
    }

    // Get the expected value from the risk factor table
    double expected = expected_->at(gender, factor).at(age);

    // Ensure the expected value is not zero or negative
    if (expected <= 0.0) {
        expected = 0.01;
    }

    // Apply trend if requested and trend data exists
    if (apply_trend && expected_trend_ && expected_trend_->contains(factor)) {
        int elapsed_time = context.time_now() - context.start_time();
        int steps = get_trend_steps(factor);
        int t = std::min<int>(elapsed_time, steps);
        expected *= std::pow(expected_trend_->at(factor), t);
    }

    // Clamp to range if provided
    if (range.has_value()) {
        expected = range.value().clamp(expected);
    }

    // Final check to ensure value is positive
    return std::max(0.01, expected);
}

int DemographicModule::get_trend_steps(const core::Identifier &factor) const noexcept {
    if (trend_steps_ && trend_steps_->contains(factor)) {
        return trend_steps_->at(factor);
    }
    return 0; // Default to no trend steps if not specified
}

std::unique_ptr<DemographicModule> build_population_module(Repository &repository,
                                                           const ModelInput &config) {
    auto min_time = config.start_time();
    auto max_time = config.stop_time();

    auto time_filter = [&min_time, &max_time](unsigned int value) {
        return value >= min_time && value <= max_time;
    };

    auto pop_data = repository.manager().get_population(config.settings().country(), time_filter);
    auto births =
        repository.manager().get_birth_indicators(config.settings().country(), time_filter);
    auto deaths = repository.manager().get_mortality(config.settings().country(), time_filter);
    auto life_table = detail::StoreConverter::to_life_table(births, deaths);
    auto population_records = detail::StoreConverter::to_population_records(pop_data);

    // Load income models from static model definition
    auto income_models = std::unordered_map<core::Income, LinearModelParams>();
    auto &static_model = repository.get_risk_factor_model_definition(RiskFactorModelType::Static);
    if (auto *static_linear_model =
            dynamic_cast<const StaticLinearModelDefinition *>(&static_model)) {
        income_models = static_linear_model->income_models();
    }

    auto region_models = std::make_shared<std::unordered_map<core::Region, LinearModelParams>>();
    auto ethnicity_models =
        std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>();

    return std::make_unique<DemographicModule>(
        std::move(population_records), std::move(life_table), std::move(income_models),
        std::move(region_models), std::move(ethnicity_models),
        0.5,       // income_continuous_stddev
        0.27510807 // physical_activity_stddev - Using the value from config
    );
}

void DemographicModule::update_population([[maybe_unused]] RuntimeContext &context) {
    // This is the base class version, which should not be called directly
    throw std::runtime_error(
        "DemographicModule::update_population(RuntimeContext&) should not be called directly. Use "
        "update_population(RuntimeContext&, const DiseaseModule&) instead.");
}

GenderTable<int, double>
DemographicModule::calculate_residual_mortality(RuntimeContext &context,
                                                const DiseaseModule &disease_host) {
    auto excess_mortality_product = create_integer_gender_table<double>(life_table_.age_limits());
    auto excess_mortality_count = create_integer_gender_table<int>(life_table_.age_limits());
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};

    // Process each person with proper exception handling
    tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        if (entity.age < 0 || entity.age > 110) {
            return;
        }

        // Calculate the excess mortality product
        double product = calculate_excess_mortality_product(entity, disease_host);

        // Safely update the tables
        auto lock = std::unique_lock{sum_mutex};
        excess_mortality_product.at(entity.age, entity.gender) += product;
        excess_mortality_count.at(entity.age, entity.gender)++;
    });

    auto death_rates = create_death_rates_table(context.time_now());
    auto residual_mortality = create_integer_gender_table<double>(life_table_.age_limits());
    auto start_age = life_table_.age_limits().lower();
    auto end_age = life_table_.age_limits().upper();
    auto default_average = 1.0;

    // Fill in the mortality table
    for (int age = start_age; age <= end_age; age++) {
        auto male_average_product = default_average;
        auto female_average_product = default_average;

        // Process males
        if (excess_mortality_count.contains(age) && death_rates.contains(age)) {
            auto male_count = excess_mortality_count.at(age, core::Gender::male);
            if (male_count > 0) {
                male_average_product =
                    excess_mortality_product.at(age, core::Gender::male) / male_count;
            }

            // Calculate mortality with safeguards against division by zero
            double male_mortality = 0.0;
            if (std::abs(male_average_product) > 1e-6) {
                male_mortality =
                    1.0 - (1.0 - death_rates.at(age, core::Gender::male)) / male_average_product;
            } else {
                male_mortality = death_rates.at(age, core::Gender::male);
            }

            // Ensure mortality is in valid range
            residual_mortality.at(age, core::Gender::male) =
                std::max(std::min(male_mortality, 1.0), 0.0);
        } else {
            // Use small default value if data is missing
            residual_mortality.at(age, core::Gender::male) = 0.001 + (age / 1000.0);
        }

        // Process females
        if (excess_mortality_count.contains(age) && death_rates.contains(age)) {
            auto female_count = excess_mortality_count.at(age, core::Gender::female);
            if (female_count > 0) {
                female_average_product =
                    excess_mortality_product.at(age, core::Gender::female) / female_count;
            }

            // Calculate mortality with safeguards against division by zero
            double female_mortality = 0.0;
            if (std::abs(female_average_product) > 1e-6) {
                female_mortality = 1.0 - (1.0 - death_rates.at(age, core::Gender::female)) /
                                             female_average_product;
            } else {
                female_mortality = death_rates.at(age, core::Gender::female);
            }

            // Ensure mortality is in valid range
            residual_mortality.at(age, core::Gender::female) =
                std::max(std::min(female_mortality, 1.0), 0.0);
        } else {
            // Use small default value if data is missing
            residual_mortality.at(age, core::Gender::female) = 0.001 + (age / 1200.0);
        }
    }

    return residual_mortality;
}
} // namespace hgps
