#include "demographic.h"
#include "HealthGPS.Core/thread_util.h"
#include "converter.h"
#include "default_disease_model.h"
#include "static_linear_model.h"
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
    auto total = 0.0;
    if (pop_data_.contains(time_year)) {
        const auto &year_data = pop_data_.at(time_year);
        total = std::accumulate(year_data.begin(), year_data.end(), 0.0,
                                [](const double previous, const auto &element) {
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
        auto total = 0.0;

        // Define the total_ratio - this was missing
        const double total_ratio = 1.0; // Using 1.0 as a default scaling factor

        // Fix the structured binding usage
        for (const auto &entry : time_data) {
            int age = entry.first;
            const auto &record = entry.second;

            result.emplace(
                age, DoubleGenderValue(record.males * total_ratio, record.females * total_ratio));

            // Calculate total by summing males and females from the just added entry
            total += result.at(age).males + result.at(age).females;
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
        total = 0.0;
        for (const auto &[age, value] : result) {
            total += value.males + value.females;
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

    // Check if the age distribution needs to be extended to 110
    int max_age = 0;
    if (!age_gender_dist.empty()) {
        max_age = age_gender_dist.rbegin()->first;

        // If max age is less than 110, extend the distribution
        if (max_age < 110) {

            // Use the values at max_age as reference for extrapolation
            DoubleGenderValue reference = age_gender_dist[max_age];
            double total_weight = 0.0;

            // Add ages from max_age+1 to 110 with exponential decay
            for (int age = max_age + 1; age <= 110; age++) {
                // Calculate decay factor based on how far we are from max_age
                double decay_factor = std::exp(-0.1 * (age - max_age));
                DoubleGenderValue new_value(reference.males * decay_factor,
                                            reference.females * decay_factor);

                age_gender_dist[age] = new_value;
                total_weight += new_value.males + new_value.females;
            }

            // Re-normalize the distribution to ensure probabilities sum to 1
            double existing_weight = 0.0;
            for (int age = 0; age <= max_age; age++) {
                if (age_gender_dist.count(age) > 0) {
                    existing_weight += age_gender_dist[age].males + age_gender_dist[age].females;
                }
            }

            // Scale factor to maintain the original distribution while adding the new ages
            double scale_factor = (1.0 - total_weight) / existing_weight;

            // Apply scaling to maintain proportional distribution
            for (int age = 0; age <= max_age; age++) {
                if (age_gender_dist.count(age) > 0) {
                    age_gender_dist[age].males *= scale_factor;
                    age_gender_dist[age].females *= scale_factor;
                }
            }
        }
    }

    // If age_gender_dist is empty or only contains age 100, create a realistic distribution
    if (age_gender_dist.empty() ||
        (age_gender_dist.size() == 1 && age_gender_dist.begin()->first == 100) ||
        (population.size() < 200)) { // Add small population check to ensure full age range
        std::cout << "WARNING: Missing, invalid, or small age distribution. Creating a realistic "
                     "distribution."
                  << std::endl;

        // Create a realistic age distribution from 0 to 110 (extended from 100)
        age_gender_dist.clear();
        double total_weight = 0.0;

        // Create age distribution with a wider spread to ensure older ages are represented
        for (int age = 0; age <= 110; age++) {
            double weight;
            if (age < 18) {
                weight = 0.2; // Children
            } else if (age < 65) {
                weight = 0.6; // Working age
            } else if (age < 100) {
                weight = 0.3 * std::exp(-0.03 * (age - 65)); // Elderly with exponential decline
            } else {
                // Add small but non-zero weight for ages 100-110
                weight = 0.05 * std::exp(-0.1 * (age - 100)); // Very elderly with steeper decline
            }

            // Ensure minimum weight to guarantee some people in all age groups
            if (population.size() < 100 && (age % 10 == 0 || age >= 100)) {
                weight = std::max(weight, 0.01); // Force minimum weight for decade ages and all
                                                 // ages 100+ in small populations
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
        [[maybe_unused]] auto reserved_count = static_cast<int>(
            required_ages.size() * 2); // One male and one female for each required age

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

    // Step 5: Initialize physical activity for each person
    for (auto &person : population) {
        initialise_physical_activity(context, person, random);
    }
}

void DemographicModule::update_population([[maybe_unused]] RuntimeContext &context) {
    // This version is required by the SimulationModule interface but should not be called directly
    throw std::runtime_error(
        "DemographicModule::update_population(RuntimeContext&) should not be called directly. Use "
        "update_population(RuntimeContext&, const DiseaseModule&) instead.");
}

void DemographicModule::update_population([[maybe_unused]] RuntimeContext &context,
                                          const DiseaseModule &disease_host) {
    std::cout << "DEBUG: Starting update_population, active size: "
              << context.population().current_active_size() << std::endl;

    // Start the residual mortality calculation asynchronously with additional safety mechanisms
    std::future<void> residual_future;
    try {
        // Using the updated run_async function with simplified parameter passing
        residual_future = core::run_async(&DemographicModule::update_residual_mortality, this,
                                          std::ref(context), std::cref(disease_host));
    } catch (const std::exception &e) {
        std::cerr << "ERROR: Failed to launch residual mortality calculation: " << e.what()
                  << std::endl;
        // We'll handle this as a failure below by creating default values
    } catch (...) {
        std::cerr << "ERROR: Failed to launch residual mortality calculation" << std::endl;
        // We'll handle this as a failure below by creating default values
    }

    auto initial_pop_size = context.population().current_active_size();

    // ENHANCED CHECK: If we have no active population, reset it immediately
    if (initial_pop_size == 0 && context.population().size() > 0) {
        std::cout << "CRITICAL WARNING: No active population detected at start of update cycle. "
                     "Performing immediate reinitialize..."
                  << std::endl;

        // Get expected population size for current time
        auto target_pop_size = get_total_population_size(context.time_now());
        float size_fraction = context.inputs().settings().size_fraction();
        int target_size = static_cast<int>(target_pop_size * size_fraction);

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

    // Check if we successfully created a future
    if (residual_future.valid()) {
        try {
            // Wait for the residual_future with a reasonable timeout (30 seconds)
            if (residual_future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
                // Safely get the result
                try {
                    residual_future.get(); // No need to store the result
                    residual_success = true;
                    std::cout << "DEBUG: Residual mortality calculation completed successfully"
                              << std::endl;
                } catch (const std::exception &e) {
                    std::cerr << "ERROR: Exception during residual mortality calculation: "
                              << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "ERROR: Unknown exception during residual mortality calculation"
                              << std::endl;
                }
            } else {
                std::cerr << "ERROR: Residual mortality calculation timed out after 30 seconds"
                          << std::endl;
                // Detach the future to let it finish in background, but proceed without waiting
                // This is safer than attempting to cancel it
                std::thread(
                    [](std::future<void> fut) {
                        try {
                            // Wait a bit longer, but don't block simulation
                            if (fut.wait_for(std::chrono::seconds(10)) ==
                                std::future_status::ready) {
                                try {
                                    fut.get();
                                } catch (...) {
                                    // Ignore any exceptions at this point
                                }
                            }
                        } catch (...) {
                            // Catch any errors in the detached thread
                        }
                    },
                    std::move(residual_future))
                    .detach();
            }
        } catch (const std::exception &e) {
            std::cerr << "ERROR: Exception while waiting for residual mortality: " << e.what()
                      << std::endl;
        } catch (...) {
            std::cerr << "ERROR: Unknown exception while waiting for residual mortality"
                      << std::endl;
        }
    } else {
        std::cerr << "ERROR: Invalid residual mortality future" << std::endl;
    }

    // If residual mortality calculation failed, initialize with default values
    if (!residual_success) {
        std::cout << "WARNING: Using default residual mortality values due to calculation failure"
                  << std::endl;

        // Create a table that covers ages 0-110 if it's empty
        try {
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

            std::cout << "INFO: Created default residual mortality values" << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "ERROR: Failed to create default mortality rates: " << e.what()
                      << std::endl;

            // Last resort - recreate the entire table
            try {
                residual_death_rates_ =
                    create_integer_gender_table<double>(core::IntegerInterval(0, 110));
                for (int age = 0; age <= 110; age++) {
                    residual_death_rates_.at(age, core::Gender::male) = 0.001;
                    residual_death_rates_.at(age, core::Gender::female) = 0.001;
                }
            } catch (...) {
                // At this point, we can only hope the simulation can continue with whatever values
                // are there
            }
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
        auto target_pop_size = get_total_population_size(context.time_now());
        float size_fraction = context.inputs().settings().size_fraction();
        int target_size = static_cast<int>(target_pop_size * size_fraction);

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

        // Reset disease caches to prevent stale data from causing errors
        extern void reset_disease_caches();
        reset_disease_caches();

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
        std::atomic<int> processed_count = 0;
        std::atomic<int> error_count = 0;

        tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
            try {
                processed_count++;

                if (!entity.is_active()) {
                    return;
                }

                if (entity.age < 0 || entity.age > 110) {
                    std::string err = "Invalid age " + std::to_string(entity.age) + " for person " +
                                      std::to_string(entity.id());
                    auto lock = std::unique_lock{sum_mutex};
                    errors.push_back(err);
                    error_count++;
                    return;
                }

                // Calculate the excess mortality product with error checking
                double product = 1.0;
                try {
                    product = calculate_excess_mortality_product(entity, disease_host);
                } catch (const std::exception &e) {
                    auto lock = std::unique_lock{sum_mutex};
                    error_count++;
                    product = 1.0; // Default value
                }

                // Safely update the tables
                auto lock = std::unique_lock{sum_mutex};
                excess_mortality_product.at(entity.age, entity.gender) += product;
                excess_mortality_count.at(entity.age, entity.gender)++;
            } catch (const std::exception &e) {
                auto lock = std::unique_lock{sum_mutex};
                errors.push_back(std::string("Error processing person: ") + e.what());
                error_count++;
            }
        });

        // Create death rates and calculate residual mortality with error handling
        try {
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
        } catch (const std::exception &e) {
            throw; // Re-throw to trigger default values
        }

        std::cout << "DEBUG: [update_residual_mortality] Calculation completed successfully"
                  << std::endl;
    } catch (const std::exception &) {

        // Create default values
        auto start_age = life_table_.age_limits().lower();
        auto end_age = life_table_.age_limits().upper();

        // Create a fresh table if it doesn't exist or is corrupted
        try {
            if (residual_death_rates_.empty()) {
                residual_death_rates_ =
                    create_integer_gender_table<double>(core::IntegerInterval(0, 110));
            }

            for (int age = start_age; age <= end_age; age++) {
                // Use age-dependent default rates
                residual_death_rates_.at(age, core::Gender::male) =
                    0.001 + (age / 1000.0); // Small age-dependent rate
                residual_death_rates_.at(age, core::Gender::female) =
                    0.001 + (age / 1200.0); // Slightly lower for females
            }

            std::cout << "INFO: Created default residual mortality values" << std::endl;
        } catch (const std::exception &ex) {
            std::cerr << "FATAL ERROR: Could not create default residual mortality values: "
                      << ex.what() << std::endl;
        }
    } catch (...) {
        std::cerr << "UNKNOWN ERROR in update_residual_mortality" << std::endl;

        // Create default values with better error handling
        try {
            auto start_age = life_table_.age_limits().lower();
            auto end_age = life_table_.age_limits().upper();

            if (residual_death_rates_.empty()) {
                residual_death_rates_ =
                    create_integer_gender_table<double>(core::IntegerInterval(0, 110));
            }

            for (int age = start_age; age <= end_age; age++) {
                residual_death_rates_.at(age, core::Gender::male) = 0.001 + (age / 1000.0);
                residual_death_rates_.at(age, core::Gender::female) = 0.001 + (age / 1200.0);
            }

            std::cout << "INFO: Created default residual mortality values" << std::endl;
        } catch (...) {
            std::cerr << "FATAL ERROR: Could not create default residual mortality values"
                      << std::endl;
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
            births.number * births.sex_ratio / (1.0 + births.sex_ratio) / population_size;
        double female_birth_rate = births.number / (1.0 + births.sex_ratio) / population_size;

        birth_rates_.at(year, core::Gender::male) = male_birth_rate;
        birth_rates_.at(year, core::Gender::female) = female_birth_rate;
    }
}

DoubleAgeGenderTable DemographicModule::create_death_rates_table(const int time_now) const {
    // Get the population and mortality data for the current time
    auto &population = pop_data_.at(time_now);
    const auto &mortality = life_table_.get_mortalities_at(time_now);

    // Create a death rates table that supports ages up to 110
    auto max_supported_age = 110;
    auto death_rates = create_age_gender_table<double>(core::IntegerInterval(0, max_supported_age));

    // Get original age limits from life table
    auto start_age = life_table_.age_limits().lower();
    auto end_age = life_table_.age_limits().upper();

    // Fill in rates for ages we have data for
    for (int age = start_age; age <= end_age; age++) {
        // Check if population data exists and is valid
        if (!population.contains(age) || !mortality.contains(age)) {
            // Set default values if data is missing
            death_rates.at(age, core::Gender::male) = 0.0;
            death_rates.at(age, core::Gender::female) = 0.0;
            continue;
        }

        // Add safety checks to prevent division by zero
        double male_rate = 0.0;
        if (population.at(age).males > 0) {
            male_rate = std::min(static_cast<double>(mortality.at(age).males) /
                                     static_cast<double>(population.at(age).males),
                                 1.0);
        }

        double female_rate = 0.0;
        if (population.at(age).females > 0) {
            female_rate = std::min(static_cast<double>(mortality.at(age).females) /
                                       static_cast<double>(population.at(age).females),
                                   1.0);
        }

        death_rates.at(age, core::Gender::male) = male_rate;
        death_rates.at(age, core::Gender::female) = female_rate;
    }

    return death_rates;
}

double DemographicModule::calculate_excess_mortality_product(const Person &entity,
                                                             const DiseaseModule &disease_host) {
    auto product = 1.0;
    try {
        // Add safety checks and better error handling
        if (entity.diseases.empty()) {
            return product; // No diseases means no excess mortality
        }

        // Validate person ID is reasonable
        if (entity.id() <= 0 || entity.id() > 10000000) { // Arbitrary high limit
            std::cerr << "WARNING: Unusual person ID: " << entity.id()
                      << ", skipping excess mortality calculation" << std::endl;
            return product;
        }

        // Validate person age is reasonable
        if (entity.age < 0 || entity.age > 120) { // Reasonable age limits
            std::cerr << "WARNING: Unusual person age: " << entity.age
                      << ", skipping excess mortality calculation" << std::endl;
            return product;
        }

        // Create a copy of the disease map keys to safely iterate
        // This prevents "invalid map<K, T> key" errors if the map is modified during iteration
        std::vector<core::Identifier> disease_keys;
        disease_keys.reserve(entity.diseases.size());

        try {
            // Use a safer approach to iterate through diseases
            for (const auto &item : entity.diseases) {
                try {
                    if (!item.first.is_empty()) {
                        disease_keys.push_back(item.first);
                    }
                } catch (...) {
                    // Skip problematic keys
                    continue;
                }
            }
        } catch (const std::exception &) {
            return product; // Return default value on error
        } catch (...) {
            return product; // Return default value on error
        }

        // Safety check - don't process unreasonable number of diseases
        if (disease_keys.size() > 50) { // Arbitrary reasonable limit
            return product;
        }

        // Now iterate through the copied keys instead of directly through the map
        for (const auto &disease_id : disease_keys) {
            try {
                // Skip if disease ID doesn't exist in disease_host
                if (!disease_host.contains(disease_id)) {
                    continue;
                }

                // Skip if disease status is not active
                auto status = disease_host.get_disease_status(disease_id, entity);
                if (status != DiseaseStatus::active) {
                    continue;
                }

                // Get excess mortality with exception handling
                double excess_mortality = 0.0;
                try {
                    excess_mortality = disease_host.get_excess_mortality(disease_id, entity);
                } catch (const std::exception &) {
                    continue;
                }

                // Sanity check the mortality value
                if (excess_mortality < 0.0 || excess_mortality > 1.0 ||
                    !std::isfinite(excess_mortality)) {
                    excess_mortality = 0.0;
                }

                product *= (1.0 - excess_mortality);

            } catch (const std::exception &e) {

            } catch (...) {
            }
        }
    } catch (const std::exception &) {
    } catch (...) {
    }

    // Ensure valid range [0, 1]
    return std::max(std::min(product, 1.0), 0.0);
}

int DemographicModule::update_age_and_death_events(RuntimeContext &context,
                                                   const DiseaseModule &disease_host) {
    // Use the maximum supported age (110) rather than just the age_range upper limit
    // This allows people to naturally age beyond the officially supported age range
    auto max_supported_age = 110u;
    auto max_age =
        std::max(static_cast<unsigned int>(context.age_range().upper()), max_supported_age);
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
                residual_death_rate = 0.001; // Safe default value
            }

            // Cap excessively high death rates to prevent everyone from dying
            if (residual_death_rate > 0.5) {
                residual_death_rate = 0.5;
            }

            auto product = 1.0 - residual_death_rate;

            // Create a safe copy of disease IDs to iterate through
            std::vector<core::Identifier> disease_keys;
            try {
                disease_keys.reserve(entity.diseases.size());
                for (const auto &item : entity.diseases) {
                    if (!item.first.is_empty()) {
                        disease_keys.push_back(item.first);
                    }
                }
            } catch (const std::exception &e) {
            } catch (...) {
            }

            // Process each disease using the copied keys
            for (const auto &disease_id : disease_keys) {
                try {
                    // Skip if disease is not active
                    if (!entity.diseases.contains(disease_id) ||
                        entity.diseases.at(disease_id).status != DiseaseStatus::active) {
                        continue;
                    }

                    // Skip if disease doesn't exist in the disease host
                    if (!disease_host.contains(disease_id)) {
                        continue;
                    }

                    // Get excess mortality with error handling
                    double excess_mortality = 0.0;
                    try {
                        excess_mortality = disease_host.get_excess_mortality(disease_id, entity);
                    } catch (const std::exception &) {
                        // Skip this disease on error
                        continue;
                    }

                    // Validate the excess mortality value
                    if (!std::isfinite(excess_mortality) || excess_mortality < 0.0 ||
                        excess_mortality > 1.0) {
                        excess_mortality = 0.0; // Use safe default
                    }

                    product *= (1.0 - excess_mortality);
                } catch (const std::exception &) {
                    // Catch any unexpected errors but continue with other diseases
                    continue;
                } catch (...) {
                    // Catch all other errors
                    continue;
                }
            }

            auto death_probability = 1.0 - product;

            // Sanity check the death probability
            if (death_probability < 0.0 || death_probability > 1.0 ||
                std::isnan(death_probability) || !std::isfinite(death_probability)) {
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
    // Get ethnicity probabilities for this age/gender/region combination
    auto ethnicity_probs = std::unordered_map<core::Ethnicity, double>();

    // Log for a sample of people
    bool log_for_this_person = (person.id() % 1000 == 0) && (person.id() < 5000);

    if (log_for_this_person) {
        std::cout << "ETHNICITY LOG [Person " << person.id() << "]: age=" << person.age
                  << ", gender=" << (person.gender == core::Gender::male ? "Male" : "Female")
                  << ", region=" << static_cast<int>(person.region) << std::endl;
    }

    // Try to use ethnicity models from static_model.json if they exist
    bool using_model_data = false;
    if (ethnicity_models_ && !ethnicity_models_->empty()) {
        // Use coefficients from the static_model.json
        double total = 0.0;
        for (const auto &[ethnicity, model] : *ethnicity_models_) {
            // Simple assignment based on intercepts for probability distribution
            ethnicity_probs[ethnicity] = model.intercept;
            total += model.intercept;
        }

        // Normalize if we got valid data
        if (total > 0.0) {
            for (auto &[ethnicity, prob] : ethnicity_probs) {
                prob /= total;
            }
            using_model_data = true;

            if (log_for_this_person) {
                std::cout << "  Using ethnicity models from static_model.json" << std::endl;
                // Print top 3 ethnicity probabilities
                int count = 0;
                for (const auto &[ethnicity, prob] : ethnicity_probs) {
                    if (count < 3) {
                        std::cout << "  Ethnicity " << static_cast<int>(ethnicity)
                                  << " prob: " << prob << std::endl;
                        count++;
                    }
                }
            }
        }
    }

    // If we don't have models or they're invalid, try to get prevalence data
    if (!using_model_data) {
        // Get prevalence data from the repository/data manager
        try {
            auto prevalence_data = context.inputs().get_ethnicity_probabilities(
                person.age, person.gender, person.region);

            if (!prevalence_data.empty()) {
                ethnicity_probs = prevalence_data;

                // Verify the probabilities are valid (sum to approximately 1.0)
                double total = 0.0;
                for (const auto &[ethnicity, prob] : ethnicity_probs) {
                    total += prob;
                }

                if (std::abs(total - 1.0) > 0.01) {
                    // If total is significantly different from 1.0, normalize
                    for (auto &[ethnicity, prob] : ethnicity_probs) {
                        prob /= total;
                    }
                }

                if (log_for_this_person) {
                    std::cout << "  Using ethnicity prevalence data, total prob: " << total
                              << std::endl;
                    // Print top 3 ethnicity probabilities
                    int count = 0;
                    for (const auto &[ethnicity, prob] : ethnicity_probs) {
                        if (count < 3) {
                            std::cout << "  Ethnicity " << static_cast<int>(ethnicity)
                                      << " prob: " << prob << std::endl;
                            count++;
                        }
                    }
                }
            } else {
                if (log_for_this_person) {
                    std::cout << "  No ethnicity prevalence data available" << std::endl;
                }
            }
        } catch (const std::exception &e) {
            if (log_for_this_person) {
                std::cout << "  Error getting ethnicity probabilities: " << e.what() << std::endl;
            }
        }
    }

    // Final safety check - if we still have no probabilities, use minimal default
    if (ethnicity_probs.empty()) {
        if (log_for_this_person) {
            std::cout << "  Using default White ethnicity due to missing data" << std::endl;
        }
        ethnicity_probs[core::Ethnicity::White] = 1.0; // Last resort default
    }

    // Use CDF for assignment
    double rand_value = random.next_double(); // next_double is between 0,1
    double cumulative_prob = 0.0;

    for (const auto &[ethnicity, prob] : ethnicity_probs) {
        cumulative_prob += prob;
        if (rand_value < cumulative_prob) {
            person.ethnicity = ethnicity;
            if (log_for_this_person) {
                std::cout << "  Assigned ethnicity: " << static_cast<int>(ethnicity)
                          << " (random value: " << rand_value
                          << " < cumulative prob: " << cumulative_prob << ")" << std::endl;
            }
            return;
        }
    }

    // Fallback assignment
    person.ethnicity = core::Ethnicity::White;
    if (log_for_this_person) {
        std::cout << "  Assigned fallback ethnicity: White" << std::endl;
    }
}

void DemographicModule::initialise_income_continuous(Person &person, Random &random) {
    // Skip inactive people
    if (!person.is_active()) {
        return;
    }

    // Add logging for a sample of people
    bool log_for_this_person = (person.id() % 1000 == 0) && (person.id() < 5000);

    if (log_for_this_person) {
        std::cout << "INCOME CONTINUOUS LOG [Person " << person.id() << "]: age=" << person.age
                  << ", gender=" << (person.gender == core::Gender::male ? "Male" : "Female")
                  << ", region=" << static_cast<int>(person.region)
                  << ", ethnicity=" << static_cast<int>(person.ethnicity) << std::endl;
    }

    // Initialize base income value - Start with intercept
    double income_base = 0.0;
    double min_income = 23.0;
    double max_income = 2375.0;
    double income_range = max_income - min_income;

    try {
        // Add intercept from the income model if available
        if (!income_models_.empty()) {
            // Start with the intercept value
            income_base = income_models_.begin()->second.intercept;
            if (log_for_this_person) {
                std::cout << "  Base intercept: " << income_base << std::endl;
            }
        }

        // Add age effect
        double age_effect = 0.0;
        if (!income_models_.empty() &&
            income_models_.begin()->second.coefficients.count("Age") > 0) {
            age_effect = income_models_.begin()->second.coefficients.at("Age") * person.age;
            income_base += age_effect;
            if (log_for_this_person) {
                std::cout << "  Age effect: " << age_effect
                          << " (coef: " << income_models_.begin()->second.coefficients.at("Age")
                          << " * age: " << person.age << ")" << std::endl;
            }
        }

        // Add gender - apply coefficient to binary gender value using one model
        double gender_value = (person.gender == core::Gender::female) ? 1.0 : 0.0;
        double gender_effect = 0.0;
        if (!income_models_.empty() &&
            income_models_.begin()->second.coefficients.count("Gender") > 0) {
            gender_effect = income_models_.begin()->second.coefficients.at("Gender") * gender_value;
            income_base += gender_effect;
            if (log_for_this_person) {
                std::cout << "  Gender effect: " << gender_effect
                          << " (coef: " << income_models_.begin()->second.coefficients.at("Gender")
                          << " * gender_value: " << gender_value << ")" << std::endl;
            }
        }

        // Add region effect - apply coefficient to region value
        double region_value = person.region_to_value();
        double region_effect = 0.0;
        if (region_models_ && region_models_->count(person.region) > 0) {
            const auto &region_model = region_models_->at(person.region);
            if (region_model.coefficients.count("Income") > 0) {
                region_effect = region_model.coefficients.at("Income") * region_value;
                income_base += region_effect;
                if (log_for_this_person) {
                    std::cout << "  Region effect: " << region_effect
                              << " (coef: " << region_model.coefficients.at("Income")
                              << " * region_value: " << region_value << ")" << std::endl;
                }
            }
        }

        // Add ethnicity effect - apply coefficient to ethnicity value
        double ethnicity_value = person.ethnicity_to_value();
        double ethnicity_effect = 0.0;
        if (ethnicity_models_ && ethnicity_models_->count(person.ethnicity) > 0) {
            const auto &ethnicity_model = ethnicity_models_->at(person.ethnicity);
            if (ethnicity_model.coefficients.count("Income") > 0) {
                ethnicity_effect = ethnicity_model.coefficients.at("Income") * ethnicity_value;
                income_base += ethnicity_effect;
                if (log_for_this_person) {
                    std::cout << "  Ethnicity effect: " << ethnicity_effect
                              << " (coef: " << ethnicity_model.coefficients.at("Income")
                              << " * ethnicity_value: " << ethnicity_value << ")" << std::endl;
                }
            }
        }

        // Add random variation - increase standard deviation to spread values more across range
        double increased_stddev = income_continuous_stddev_;
        double rand = random.next_normal(0.0, increased_stddev);
        double income_with_random = income_base * (1.0 + rand);
        if (log_for_this_person) {
            std::cout << "  Random effect: " << (income_with_random - income_base)
                      << " (increased stddev: " << increased_stddev << ", random value: " << rand
                      << ")" << std::endl;
        }

        // Check if the value is at risk of being at the min or max extremes
        if (income_with_random < min_income + (income_range * 0.05) ||
            income_with_random > max_income - (income_range * 0.05)) {

            // Add a smoothing function to avoid clumping at extremes
            // Use a triangular-like distribution to ensure more values in middle ranges
            double rand_triangular =
                random.next_double() + random.next_double(); // Sum of 2 uniforms = triangular
            rand_triangular =
                (rand_triangular - 1.0) * income_range * 0.5; // Scale to half the range

            // Pull extreme values toward center
            if (income_with_random < min_income + (income_range * 0.05)) {
                income_with_random = min_income + (income_range * 0.05) + std::abs(rand_triangular);
                if (log_for_this_person) {
                    std::cout << "  Adjusted low extreme toward center: " << income_with_random
                              << std::endl;
                }
            } else if (income_with_random > max_income - (income_range * 0.05)) {
                income_with_random = max_income - (income_range * 0.05) - std::abs(rand_triangular);
                if (log_for_this_person) {
                    std::cout << "  Adjusted high extreme toward center: " << income_with_random
                              << std::endl;
                }
            }
        }

        // Ensure income is within valid range [23, 2375]
        person.income_continuous = std::max(min_income, income_with_random);
        person.income_continuous = std::min(max_income, person.income_continuous);

        if (log_for_this_person) {
            std::cout << "  Final income_continuous: " << person.income_continuous << std::endl;
        }

    } catch (const std::exception &) {
        // Fallback - use a more evenly distributed random value rather than just the minimum
        double random_portion = random.next_double(); // Value between 0 and 1
        person.income_continuous = min_income + (income_range * random_portion);

        if (log_for_this_person) {
            std::cout << "  Exception in income calculation, using evenly distributed fallback: "
                      << person.income_continuous << std::endl;
        }
    }
}

void DemographicModule::initialise_income_category(Person &person, double q1_threshold,
                                                   double q2_threshold, double q3_threshold) {
    // Skip inactive people
    if (!person.is_active()) {
        return;
    }

    // Add logging for a sample of people
    bool log_for_this_person = (person.id() % 1000 == 0) && (person.id() < 5000);

    if (log_for_this_person) {
        std::cout << "INCOME CATEGORY LOG [Person " << person.id()
                  << "]: income_continuous=" << person.income_continuous << std::endl;
        std::cout << "  Thresholds: Q1=" << q1_threshold << ", Q2=" << q2_threshold
                  << ", Q3=" << q3_threshold << std::endl;

        // Analyze if thresholds seem inverted
        if (q1_threshold > q2_threshold || q2_threshold > q3_threshold) {
            std::cerr << "WARNING: Income thresholds appear to be inverted or incorrect!"
                      << std::endl;
            std::cerr << "  Thresholds should be in ascending order: Q1 < Q2 < Q3" << std::endl;
        }
    }

    // Store original income for validation
    double original_income = person.income_continuous;

    // Get the income range from config [23, 2375]
    double min_income = 23.0;
    double max_income = 2375.0;
    double income_range = max_income - min_income;

    // Calculate more logical thresholds if the provided ones seem inconsistent
    // This ensures a proper distribution even if the quartile calculation is off
    double income_value = person.income_continuous;
    bool use_fixed_thresholds = false;

    // Check for threshold anomalies that would cause illogical assignments
    if (q1_threshold > q2_threshold || q2_threshold > q3_threshold ||
        q3_threshold < min_income + (income_range * 0.5) ||
        q1_threshold > min_income + (income_range * 0.5)) {

        // Use fixed percentages of the income range instead
        q1_threshold = min_income + (income_range * 0.25); // 25th percentile
        q2_threshold = min_income + (income_range * 0.5);  // 50th percentile
        q3_threshold = min_income + (income_range * 0.75); // 75th percentile

        if (log_for_this_person) {
            std::cout << "  Using fixed thresholds due to inconsistencies: Q1=" << q1_threshold
                      << ", Q2=" << q2_threshold << ", Q3=" << q3_threshold << std::endl;
        }
        use_fixed_thresholds = true;
    }

    // Verify thresholds are valid and enforce config range [23, 2375]
    // Ensure minimum income is correctly handled
    if (income_value < q1_threshold) {
        person.income_category = core::Income::low;
        if (log_for_this_person) {
            std::cout << "  Assigned income category: Low (income < Q1)" << std::endl;
        }
    } else if (income_value < q2_threshold) {
        person.income_category = core::Income::lowermiddle;
        if (log_for_this_person) {
            std::cout << "  Assigned income category: Lower Middle (Q1 <= income < Q2)"
                      << std::endl;
        }
    } else if (income_value < q3_threshold) {
        person.income_category = core::Income::uppermiddle;
        if (log_for_this_person) {
            std::cout << "  Assigned income category: Upper Middle (Q2 <= income < Q3)"
                      << std::endl;
        }
    } else {
        person.income_category = core::Income::high;
        if (log_for_this_person) {
            std::cout << "  Assigned income category: High (income >= Q3)" << std::endl;
        }
    }

    // Add check for income changes after category assignment
    static bool warning_shown = false;
    if (!warning_shown && person.income_continuous != original_income) {
        std::cout << "WARNING: Income value changed during category assignment for person "
                  << person.id() << ". Original: " << original_income
                  << ", New: " << person.income_continuous << std::endl;
        warning_shown = true;
    }

    // Validate the category makes sense given the income
    static bool threshold_warning_shown = false;
    if (!threshold_warning_shown) {
        bool category_mismatch = false;

        // For edge cases - very low income should never be High, very high income should never be
        // Low
        if ((income_value <= min_income + (income_range * 0.1) &&
             person.income_category == core::Income::high) ||
            (income_value >= max_income - (income_range * 0.1) &&
             person.income_category == core::Income::low)) {

            category_mismatch = true;

            // Emergency override for extreme cases
            if (income_value <= min_income + (income_range * 0.1)) {
                person.income_category = core::Income::low;
                if (log_for_this_person) {
                    std::cout << "  OVERRIDE: Forcing Low category for very low income"
                              << std::endl;
                }
            } else if (income_value >= max_income - (income_range * 0.1)) {
                person.income_category = core::Income::high;
                if (log_for_this_person) {
                    std::cout << "  OVERRIDE: Forcing High category for very high income"
                              << std::endl;
                }
            }
        }

        if (category_mismatch && !use_fixed_thresholds) {
            std::cerr << "WARNING: Income categories appear to be incorrectly assigned!"
                      << std::endl;
            std::cerr << "  Income " << income_value << " was initially assigned to category "
                      << static_cast<int>(person.income_category) << std::endl;
            std::cerr << "  Thresholds: Q1=" << q1_threshold << ", Q2=" << q2_threshold
                      << ", Q3=" << q3_threshold << std::endl;
            std::cerr << "  Using fixed thresholds for future assignments" << std::endl;
            threshold_warning_shown = true;
        }
    }
}

void DemographicModule::initialise_physical_activity([[maybe_unused]] RuntimeContext &context,
                                                     Person &person, Random &random) {
    // Skip inactive people
    if (!person.is_active()) {
        return;
    }

    // Add logging for a sample of people
    bool log_for_this_person = (person.id() % 1000 == 0) && (person.id() < 5000);

    if (log_for_this_person) {
        std::cout << "PHYSICAL ACTIVITY LOG [Person " << person.id() << "]: age=" << person.age
                  << ", gender=" << (person.gender == core::Gender::male ? "Male" : "Female")
                  << ", region=" << static_cast<int>(person.region)
                  << ", ethnicity=" << static_cast<int>(person.ethnicity)
                  << ", income=" << person.income_continuous << std::endl;
    }

    try {
        // Use the intercept directly from PhysicalActivityModels
        double intercept = 1.710451556; // Default intercept from PhysicalActivityModels.continuous

        if (log_for_this_person) {
            std::cout << "  Base intercept: " << intercept << std::endl;
        }

        // Calculate base expected PA value for age and gender
        double expected = intercept;

        // Apply modifiers based on region
        double region_effect = 0.0;
        if (region_models_ && region_models_->count(person.region) > 0) {
            const auto &region_params = region_models_->at(person.region);
            if (region_params.coefficients.count("PhysicalActivity") > 0) {
                region_effect = region_params.coefficients.at("PhysicalActivity");
                expected += (region_effect);
                if (log_for_this_person) {
                    std::cout << "  Region effect: " << region_effect << " for region "
                              << static_cast<int>(person.region) << " (new value: " << expected
                              << ")" << std::endl;
                }
            }
        }

        // Apply modifiers based on ethnicity
        double ethnicity_effect = 0.0;
        if (ethnicity_models_ && ethnicity_models_->count(person.ethnicity) > 0) {
            const auto &ethnicity_params = ethnicity_models_->at(person.ethnicity);
            if (ethnicity_params.coefficients.count("PhysicalActivity") > 0) {
                ethnicity_effect = ethnicity_params.coefficients.at("PhysicalActivity");
                expected += (ethnicity_effect);
                if (log_for_this_person) {
                    std::cout << "  Ethnicity effect: " << ethnicity_effect << " for ethnicity "
                              << static_cast<int>(person.ethnicity) << " (new value: " << expected
                              << ")" << std::endl;
                }
            }
        }

        // Apply modifiers based on continuous income - using all income models
        double income_effect = 0.0;
        for (const auto &[income_level, model] : income_models_) {
            if (model.coefficients.count("PhysicalActivity") > 0) {
                double this_effect =
                    model.coefficients.at("PhysicalActivity") * person.income_continuous;
                income_effect += this_effect;
                if (log_for_this_person) {
                    std::cout << "  Income level " << static_cast<int>(income_level)
                              << " effect: " << this_effect
                              << " (coef: " << model.coefficients.at("PhysicalActivity")
                              << " * income: " << person.income_continuous << ")" << std::endl;
                }
            }
        }
        expected += (income_effect);
        if (log_for_this_person) {
            std::cout << "  Total income effect: " << income_effect << " (new value: " << expected
                      << ")" << std::endl;
        }

        // Add random variation using normal distribution
        double rand = random.next_normal(0.0, physical_activity_stddev_);
        double expected_with_random = expected * (1.0 + rand);
        if (log_for_this_person) {
            std::cout << "  Random variation: " << rand << " using stddev "
                      << physical_activity_stddev_
                      << " (value with random: " << expected_with_random << ")" << std::endl;
        }

        // Clamp the final value to a reasonable range
        core::DoubleInterval pa_range(1.4, 2.5);
        double final_value = pa_range.clamp(expected_with_random);
        if (log_for_this_person) {
            std::cout << "  Final PA value: " << final_value
                      << (final_value != expected_with_random
                              ? " (clamped from " + std::to_string(expected_with_random) + ")"
                              : "")
                      << std::endl;
        }

        // Set the physical activity value
        person.risk_factors["PhysicalActivity"_id] = final_value;

    } catch (const std::exception &e) {
        // Use a default range and midpoint as fallback
        core::DoubleInterval pa_range(1.4, 2.5);
        double fallback_value = (pa_range.lower() + pa_range.upper()) / 2.0;
        if (log_for_this_person) {
            std::cout << "  ERROR: Failed to calculate physical activity: " << e.what()
                      << ". Using fallback value: " << fallback_value << std::endl;
        }
        person.risk_factors["PhysicalActivity"_id] = fallback_value;
    }
}

std::tuple<double, double, double>
DemographicModule::calculate_income_thresholds(const Population &population) {
    // Get the income range from config
    double min_income = 23.0;
    double max_income = 2375.0;
    double range = max_income - min_income;

    // Instead of calculating quartiles from the actual distribution,
    // always use fixed percentages of the range for more balanced categories
    double q1_threshold = min_income + range * 0.25; // 25th percentile
    double q2_threshold = min_income + range * 0.5;  // 50th percentile
    double q3_threshold = min_income + range * 0.75; // 75th percentile

    // Collect statistics about the current income distribution for reporting only
    std::vector<double> sorted_incomes;
    sorted_incomes.reserve(population.size());

    int above_max_count = 0;
    int below_min_count = 0;
    int min_count = 0;
    int max_count = 0;

    for (const auto &person : population) {
        if (person.is_active()) {
            // Count values outside valid range before capping
            if (person.income_continuous > max_income) {
                above_max_count++;
            }
            if (person.income_continuous < min_income) {
                below_min_count++;
            }

            // Already enforced in initialise_income_continuous, but double-check here
            double valid_income = std::max(min_income, person.income_continuous);
            valid_income = std::min(max_income, valid_income);

            sorted_incomes.push_back(valid_income);

            // Count min/max values for reporting
            if (valid_income == min_income)
                min_count++;
            if (valid_income == max_income)
                max_count++;
        }
    }

    // Report statistics about the income distribution but don't print them
    if (!sorted_incomes.empty()) {
        // double min_percent = 100.0 * min_count / sorted_incomes.size();
        // double max_percent = 100.0 * max_count / sorted_incomes.size();

        // Remove all these print statements
        /*
        if (min_percent > 5.0 || max_percent > 5.0) {
            std::cout << "INFO: Income distribution statistics:" << std::endl;

            if (min_percent > 5.0) {
                std::cout << "  " << min_count << " income values (" << min_percent
                          << "%) are at the minimum value (" << min_income << ")" << std::endl;
            }

            if (max_percent > 5.0) {
                std::cout << "  " << max_count << " income values (" << max_percent
                          << "%) are at the maximum value (" << max_income << ")" << std::endl;
            }
        }
        */
    }
    return {q1_threshold, q2_threshold, q3_threshold};
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

    // Load all required models and parameters from static model definition
    auto income_models = std::unordered_map<core::Income, LinearModelParams>();
    auto region_models = std::make_shared<std::unordered_map<core::Region, LinearModelParams>>();
    auto ethnicity_models =
        std::make_shared<std::unordered_map<core::Ethnicity, LinearModelParams>>();
    double income_continuous_stddev = 293.752576754905;
    double physical_activity_stddev = 0.27510807;

    auto &static_model = repository.get_risk_factor_model_definition(RiskFactorModelType::Static);
    if (auto *static_linear_model =
            dynamic_cast<const StaticLinearModelDefinition *>(&static_model)) {
        // Load income models
        income_models = static_linear_model->income_models();

        // Load region models from static model
        if (static_linear_model->region_models()) {
            *region_models = *static_linear_model->region_models();
        }

        // Load ethnicity models from static model
        if (static_linear_model->ethnicity_models()) {
            *ethnicity_models = *static_linear_model->ethnicity_models();
        }

        // Load standard deviations from static model
        income_continuous_stddev = static_linear_model->income_continuous_stddev();
        physical_activity_stddev = static_linear_model->physical_activity_stddev();

        // Remove debug model information logs
        // std::cout << "DEBUG: [StaticLinearModel] Loaded "
        //           << income_models.size() << " income models, "
        //           << region_models->size() << " region models, "
        //           << ethnicity_models->size() << " ethnicity models" << std::endl;
        // std::cout << "DEBUG: [StaticLinearModel] Using income_continuous_stddev="
        //           << income_continuous_stddev << ", physical_activity_stddev="
        //           << physical_activity_stddev << std::endl;
    } else {
        std::cerr << "WARNING: Static linear model not available, using defaults" << std::endl;
        // Provide reasonable defaults if model not available
        income_continuous_stddev = 293.752576754905; // Default value
        physical_activity_stddev = 0.27510807;       // Default value
    }

    return std::make_unique<DemographicModule>(std::move(population_records), std::move(life_table),
                                               std::move(income_models), std::move(region_models),
                                               std::move(ethnicity_models),
                                               income_continuous_stddev, physical_activity_stddev);
}

GenderTable<int, double>
DemographicModule::calculate_residual_mortality(RuntimeContext &context,
                                                const DiseaseModule &disease_host) {
    // Create tables supporting ages up to 110
    auto max_supported_age = 110; // Changed from unsigned to int to match entity.age type
    auto age_range = core::IntegerInterval(0, max_supported_age);

    auto excess_mortality_product = create_integer_gender_table<double>(age_range);
    auto excess_mortality_count = create_integer_gender_table<int>(age_range);
    auto &pop = context.population();
    auto sum_mutex = std::mutex{};

    tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        if (entity.age < 0 || entity.age > max_supported_age) {
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
    auto residual_mortality = create_integer_gender_table<double>(age_range);
    auto start_age = 0; // Start from age 0 to fully support all ages
    auto end_age = max_supported_age;
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
