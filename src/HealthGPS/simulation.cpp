#include "simulation.h"
#include "HealthGPS.Core/thread_util.h"
#include "HealthGPS.Core/univariate_summary.h"
#include "converter.h"
#include "info_message.h"
#include "mtrandom.h"
#include "sync_message.h"
#include "univariate_visitor.h"
#include "runtime_context.h"

#include <algorithm>
#include <fmt/format.h>
#include <iostream>
#include <memory>
#include <oneapi/tbb/parallel_for_each.h>
#include <stdexcept>
#include <future>

namespace { // anonymous namespace

/// @brief Defines the net immigration synchronisation message
using NetImmigrationMessage = hgps::SyncDataMessage<hgps::IntegerAgeGenderTable>;

} // anonymous namespace

namespace hgps {

Simulation::Simulation(SimulationModuleFactory &factory, std::shared_ptr<const EventAggregator> bus,
                       std::shared_ptr<const ModelInput> inputs, std::unique_ptr<Scenario> scenario)
    : context_{std::move(bus), std::move(inputs), std::move(scenario)} {

    // Create required modules, should change to shared_ptr
    auto ses_base = factory.create(SimulationModuleType::SES, context_.inputs());
    auto dem_base = factory.create(SimulationModuleType::Demographic, context_.inputs());
    auto risk_base = factory.create(SimulationModuleType::RiskFactor, context_.inputs());
    auto disease_base = factory.create(SimulationModuleType::Disease, context_.inputs());
    auto analysis_base = factory.create(SimulationModuleType::Analysis, context_.inputs());

    ses_ = std::static_pointer_cast<UpdatableModule>(ses_base);
    demographic_ = std::static_pointer_cast<DemographicModule>(dem_base);
    risk_factor_ = std::static_pointer_cast<RiskFactorHostModule>(risk_base);
    disease_ = std::static_pointer_cast<DiseaseModule>(disease_base);
    analysis_ = std::static_pointer_cast<UpdatableModule>(analysis_base);
    
    // Set the demographic module in the RuntimeContext so it can be accessed 
    // via context.demographic_module() in other modules
    context_.set_demographic_module(demographic_);
}

void Simulation::setup_run(unsigned int run_number, unsigned int run_seed) noexcept {
    context_.set_current_run(run_number);
    context_.random().seed(run_seed);
}

adevs::Time Simulation::init(adevs::SimEnv<int> *env) {
    auto start = std::chrono::steady_clock::now();
    const auto &inputs = context_.inputs();
    auto world_time = inputs.start_time();
    context_.metrics().clear();
    context_.scenario().clear();
    context_.set_current_time(world_time);
    end_time_ = adevs::Time(inputs.stop_time(), 0);
    
    // Add debug output for start and end time configuration
    std::cout << "DEBUG: Simulation start time: " << world_time 
              << ", end time: " << inputs.stop_time() 
              << ", duration: " << (inputs.stop_time() - world_time) << " years" 
              << std::endl;

    // Force the end time to be at least one year after start time to ensure multiple steps
    if (end_time_.real <= world_time) {
        std::cout << "WARNING: End time <= start time, forcing end time to be start time + 5" << std::endl;
        end_time_ = adevs::Time(world_time + 5, 0);
    }

    initialise_population();

    auto stop = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(stop - start);

    auto message =
        fmt::format("[{:4},{}] population size: {}, elapsed: {}ms", env->now().real,
                    env->now().logical, context_.population().initial_size(), elapsed.count());
    context_.publish(std::make_unique<InfoEventMessage>(
        name(), ModelAction::start, context_.current_run(), context_.time_now(), message));

    // Initialize the simulation with the first event
    return adevs::Time(world_time, 0);
}

adevs::Time Simulation::update(adevs::SimEnv<int> *env) {
    // Clear progress indicator
    std::cout << "====== PROCESSING YEAR " << env->now().real << " ======" << std::endl;
    
    if (env->now() < end_time_) {
        std::cout << "DEBUG: [update] Setting up for year processing" << std::endl;
        auto start = std::chrono::steady_clock::now();
        context_.metrics().reset();

        // Now move world clock to time t + 1
        auto world_time = env->now() + adevs::Time(1, 0);
        auto time_year = world_time.real;
        context_.set_current_time(time_year);
        
        std::cout << "DEBUG: [update] About to call update_population()" << std::endl;
        update_population();
        std::cout << "DEBUG: [update] Completed update_population() successfully" << std::endl;

        auto stop = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double, std::milli>(stop - start);

        auto message = fmt::format("[{:4},{}], elapsed: {}ms", env->now().real, env->now().logical,
                                   elapsed.count());
        std::cout << "DEBUG: [update] Publishing completion message" << std::endl;
        context_.publish(std::make_unique<InfoEventMessage>(
            name(), ModelAction::update, context_.current_run(), context_.time_now(), message));

        // Mark year completion for tracking
        std::cout << "====== COMPLETED YEAR " << env->now().real << " ======" << std::endl;
        
        // CRITICAL FIX: Ensure we return a valid time for the next event
        // The simulation will stop if we don't return a valid time less than infinity
        if (world_time < end_time_) {
            std::cout << "DEBUG: [update] Scheduling next year: " << world_time.real << std::endl;
            return world_time;
        } else {
            std::cout << "===== SIMULATION END: Reached end year " << world_time.real << " =====" << std::endl;
            std::cout << "DEBUG: [update] Removing simulation model and returning infinity" << std::endl;
            env->remove(this); // Properly remove the model before returning infinity
            return adevs_inf<adevs::Time>(); // Return infinity to signal end of simulation
        }
    }

    // We have reached the end, remove the model and return infinite time for next event.
    std::cout << "===== SIMULATION END: Final year reached =====" << std::endl;
    std::cout << "DEBUG: [update] End time reached, removing simulation model" << std::endl;
    env->remove(this);
    return adevs_inf<adevs::Time>();
}

adevs::Time Simulation::update(adevs::SimEnv<int> * /*env*/, std::vector<int> & /*x*/) {
    // This method is never called because nobody sends messages.
    return adevs_inf<adevs::Time>();
}

void Simulation::fini(adevs::Time clock) {
    // risk_factor_->update_population(context_);
    auto message = fmt::format("[{:4},{}] clear up resources.", clock.real, clock.logical);
    context_.publish(std::make_unique<InfoEventMessage>(
        name(), ModelAction::stop, context_.current_run(), context_.time_now(), message));
}

void Simulation::initialise_population() {
    /* Note: order is very important */

    // Create virtual population
    const auto &inputs = context_.inputs();
    auto model_start_year = inputs.start_time();
    auto total_year_pop_size = demographic_->get_total_population_size(model_start_year);
    float size_fraction = inputs.settings().size_fraction();
    auto virtual_pop_size = static_cast<int>(size_fraction * total_year_pop_size);
    context_.reset_population(virtual_pop_size);

    // Gender - Age, must be first, followed by region, ethnicity, income, physical activity
    demographic_->initialise_population(context_, context_.population(), context_.random());

    // Social economics status
    ses_->initialise_population(context_, context_.population(), context_.random());

    // Generate risk factors
    risk_factor_->initialise_population(context_, context_.population(), context_.random());

    // Initialise diseases
    disease_->initialise_population(context_, context_.population(), context_.random());

    // Initialise analysis
    analysis_->initialise_population(context_, context_.population(), context_.random());

    print_initial_population_statistics();
}

void Simulation::update_population() {
    /* Note: order is very important */

    // update basic information: demographics + diseases
    std::cout << "DEBUG: Starting demographic updates" << std::endl;
    demographic_->update_population(context_, *disease_);
    std::cout << "DEBUG: Completed demographic updates" << std::endl;

    // Calculate the net immigration by gender and age, update the population accordingly
    std::cout << "DEBUG: Starting net immigration updates" << std::endl;
    update_net_immigration();
    std::cout << "DEBUG: Completed net immigration updates" << std::endl;

    // update population socio-economic status
    std::cout << "DEBUG: Starting socio-economic updates" << std::endl;
    ses_->update_population(context_);
    std::cout << "DEBUG: Completed socio-economic updates" << std::endl;

    // Update population risk factors
    std::cout << "DEBUG: Starting risk factor updates" << std::endl;
    try {
        // Add more detailed logging within the risk factor update process
        std::cout << "DEBUG: [Risk Factor] Beginning update process..." << std::endl;
        
        // Additional safety check on population before updates
        std::cout << "DEBUG: [Risk Factor] Population active size before update: " 
                  << context_.population().current_active_size() << std::endl;
        
        // Call the risk factor update with timeout protection
        auto risk_future = std::async(std::launch::async, [this]() {
            try {
                this->risk_factor_->update_population(context_);
                return true;
            } catch (const std::exception& e) {
                std::cerr << "ERROR during risk factor update: " << e.what() << std::endl;
                return false;
            } catch (...) {
                std::cerr << "CRITICAL ERROR: Unknown exception during risk factor update" << std::endl;
                return false;
            }
        });
        
        // Wait with timeout to prevent hanging indefinitely
        if (risk_future.wait_for(std::chrono::minutes(5)) != std::future_status::ready) {
            std::cerr << "ERROR: Risk factor update timed out after 5 minutes" << std::endl;
            // Continue with other updates even if risk factors had a problem
        } else {
            bool success = risk_future.get();
            if (success) {
                std::cout << "DEBUG: [Risk Factor] Update completed successfully" << std::endl;
            } else {
                std::cerr << "ERROR: Risk factor update returned failure" << std::endl;
            }
        }
        
        // Additional safety check on population after updates
        std::cout << "DEBUG: [Risk Factor] Population active size after update: " 
                  << context_.population().current_active_size() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR in risk factor update: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "CRITICAL ERROR: Unknown exception in risk factor update" << std::endl;
    }
    std::cout << "DEBUG: Completed risk factor updates" << std::endl;

    // Update diseases status: remission and incidence
    std::cout << "DEBUG: Starting disease status updates" << std::endl;
    try {
        // Check if any people in the population have active disease status before updating
        bool has_active_diseases = false;
        for (const auto& person : context_.population()) {
            if (person.is_active() && !person.diseases.empty()) {
                has_active_diseases = true;
                break;
            }
        }
        
        if (!has_active_diseases) {
            std::cout << "DEBUG: No active diseases found in population, skipping disease updates" << std::endl;
        } else {
            // Call the disease update method with additional safeguards
            auto disease_future = std::async(std::launch::async, [this]() {
                try {
                    this->disease_->update_population(context_);
                    return true;
                } catch (const std::exception& e) {
                    std::cerr << "ERROR during disease update: " << e.what() << std::endl;
                    return false;
                } catch (...) {
                    std::cerr << "CRITICAL ERROR: Unknown exception during disease update" << std::endl;
                    return false;
                }
            });
            
            // Wait with timeout to prevent hanging indefinitely
            if (disease_future.wait_for(std::chrono::minutes(2)) != std::future_status::ready) {
                std::cerr << "ERROR: Disease update timed out after 2 minutes" << std::endl;
            } else {
                bool success = disease_future.get();
                if (success) {
                    std::cout << "DEBUG: Disease update completed successfully" << std::endl;
                } else {
                    std::cerr << "ERROR: Disease update returned failure" << std::endl;
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR in disease update: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "CRITICAL ERROR: Unknown exception in disease update" << std::endl;
    }
    std::cout << "DEBUG: Completed disease status updates" << std::endl;

    // Publish results to data logger
    std::cout << "DEBUG: Starting analysis updates" << std::endl;
    try {
        // Check for valid ethnicity values before analysis
        int skipped_count = 0;
        for (auto& person : context_.population()) {
            if (person.is_active()) {
                // Check if ethnicity is valid
                try {
                    // This will throw if ethnicity is unknown
                    person.ethnicity_to_value();
                } catch (const std::exception&) {
                    // Don't set a default ethnicity, just log and skip this person for analysis
                    // Temporarily mark as inactive for analysis purposes
                    person.emigrate(context_.time_now());
                    skipped_count++;
                }
            }
        }
        
        if (skipped_count > 0) {
            std::cerr << "WARNING: Skipped " << skipped_count << " people with unknown ethnicity for analysis" << std::endl;
        }
        
        // Now run the analysis with improved error handling
        auto analysis_future = std::async(std::launch::async, [this]() {
            try {
                this->analysis_->update_population(context_);
                return true;
            } catch (const std::exception& e) {
                std::cerr << "ERROR during analysis: " << e.what() << std::endl;
                return false;
            } catch (...) {
                std::cerr << "CRITICAL ERROR: Unknown exception during analysis" << std::endl;
                return false;
            }
        });
        
        // Wait with timeout to prevent hanging indefinitely
        if (analysis_future.wait_for(std::chrono::minutes(2)) != std::future_status::ready) {
            std::cerr << "ERROR: Analysis timed out after 2 minutes" << std::endl;
        } else {
            bool success = analysis_future.get();
            if (success) {
                std::cout << "DEBUG: Analysis completed successfully" << std::endl;
            } else {
                std::cerr << "ERROR: Analysis returned failure" << std::endl;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR in analysis update: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "CRITICAL ERROR: Unknown exception in analysis update" << std::endl;
    }
    std::cout << "DEBUG: Completed analysis updates" << std::endl;
}

void Simulation::update_net_immigration() {
    try {
        std::cout << "DEBUG: [Net Immigration] Starting get_net_migration" << std::endl;
        
        // Get net migration data, will generate fallback data if there's an error
        auto net_immigration = get_net_migration();
        
        std::cout << "DEBUG: [Net Immigration] Got migration data, beginning population updates" << std::endl;

        // Update population based on net immigration
        auto start_age = context_.age_range().lower();
        auto end_age = context_.age_range().upper();
        
        for (int age = start_age; age <= end_age; age++) {
            try {
                // Handle males
                int male_net_value = 0;
                if (net_immigration.contains(age, core::Gender::male)) {
                    male_net_value = net_immigration.at(age, core::Gender::male);
                }
                // Removed the std::cout statement
                apply_net_migration(male_net_value, age, core::Gender::male);
            } catch (const std::exception &e) {
                std::cerr << "ERROR: Failed to process male migration for age " << age << ": "
                          << e.what() << std::endl;
            }

            try {
                // Handle females
                int female_net_value = 0;
                if (net_immigration.contains(age, core::Gender::female)) {
                    female_net_value = net_immigration.at(age, core::Gender::female);
                }
                // Removed the std::cout statement
                apply_net_migration(female_net_value, age, core::Gender::female);
            } catch (const std::exception &e) {
                std::cerr << "ERROR: Failed to process female migration for age " << age << ": "
                          << e.what() << std::endl;
            }
        }
        
        std::cout << "DEBUG: [Net Immigration] Finished applying migration to population" << std::endl;
        
        // Basic check to ensure we still have active population
        if (context_.population().current_active_size() == 0) {
            std::cout << "CRITICAL WARNING: Population became inactive after immigration. Performing emergency reset." << std::endl;
            
            // Reset population as a last resort
            auto expected_pop_size = demographic_->get_total_population_size(context_.time_now());
            float size_fraction = context_.inputs().settings().size_fraction();
            int target_size = static_cast<int>(expected_pop_size * size_fraction);
            
            // Reset and initialize
            context_.reset_population(target_size > 0 ? target_size : 100);
            initialise_population();
            
            std::cout << "Population reset with " << context_.population().current_active_size() 
                     << " active members." << std::endl;
        }
    
        // Enhanced synchronization between baseline and intervention
        if (context_.scenario().type() == ScenarioType::baseline) {
            // Baseline must send migration data to intervention
            
            // Make multiple attempts to send data to ensure intervention receives it
            const int max_send_attempts = 3;
            bool send_success = false;
            
            for (int attempt = 1; attempt <= max_send_attempts && !send_success; attempt++) {
                try {
                    // Make sure we're creating a new copy of the data to avoid issues
                    auto net_migration_copy = create_age_gender_table<int>(context_.age_range());
                    auto start_age = context_.age_range().lower();
                    auto end_age = context_.age_range().upper();
                    for (int age = start_age; age <= end_age; age++) {
                        // Check if the keys exist before using at()
                        if (net_immigration.contains(age, core::Gender::male)) {
                        net_migration_copy.at(age, core::Gender::male) = net_immigration.at(age, core::Gender::male);
                        }
                        if (net_immigration.contains(age, core::Gender::female)) {
                        net_migration_copy.at(age, core::Gender::female) = net_immigration.at(age, core::Gender::female);
                        }
                    }
                    
                    // Send with timeout protection
                    auto future = std::async(std::launch::async, [&, net_migration_copy]() mutable {
                        try {
                            // Safe send with logging
                            std::cout << "DEBUG: [Baseline] Sending migration data to intervention..." << std::endl;
                            
                            // Create a message and add verification info
                            auto message = std::make_unique<NetImmigrationMessage>(
                                context_.current_run(), context_.time_now(), std::move(net_migration_copy));
                                
                            // Send the message
                            context_.scenario().channel().send(std::move(message));
                            
                            std::cout << "DEBUG: [Baseline] Migration data sent successfully" << std::endl;
                            return true;
                        } 
                        catch (const std::exception& e) {
                            std::cerr << "ERROR during async send: " << e.what() << std::endl;
                            return false;
                        }
                    });
                    
                    // Wait for completion with timeout - increased for reliability
                    auto timeout_seconds = 10 + static_cast<int>(context_.population().initial_size() / 10000);
                    
                    if (future.wait_for(std::chrono::seconds(timeout_seconds)) == std::future_status::ready) {
                        bool success = future.get();
                        if (success) {
                            send_success = true;
                            break; // Exit the retry loop
                        } else {
                            std::cerr << "ERROR: Failed to send migration data on attempt " << attempt << std::endl;
                        }
                    } else {
                        std::cerr << "ERROR: Sending migration data timed out after " << timeout_seconds << " seconds on attempt " << attempt << std::endl;
                    }
                    
                    // Wait before retry to allow intervention to stabilize
                    if (attempt < max_send_attempts) {
                        int retry_wait = 2000 * attempt;
                        std::this_thread::sleep_for(std::chrono::milliseconds(retry_wait));
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "ERROR in baseline send attempt " << attempt << ": " << e.what() << std::endl;
                    
                    // Wait before retry
                    if (attempt < max_send_attempts) {
                        int retry_wait = 2000 * attempt;
                        std::this_thread::sleep_for(std::chrono::milliseconds(retry_wait));
                    }
                }
            }
        }
        // No else clause needed - intervention scenario already received the message in get_net_migration
    } 
    catch (const std::exception& e) {
        // Log the error but continue execution
        std::cerr << "ERROR: Exception in update_net_immigration: " << e.what() << std::endl;
    }
}

IntegerAgeGenderTable Simulation::get_current_expected_population() const {
    try {
        const auto &inputs = context_.inputs();
        auto sim_start_time = context_.start_time();
        auto total_initial_population = demographic_->get_total_population_size(sim_start_time);
        float size_fraction = inputs.settings().size_fraction();
        auto start_population_size = static_cast<int>(size_fraction * total_initial_population);

        const auto &current_population_table =
            demographic_->get_population_distribution(context_.time_now());
        auto expected_population = create_age_gender_table<int>(context_.age_range());
        auto start_age = context_.age_range().lower();
        auto end_age = context_.age_range().upper();
        
        // Find the highest available age in the population table
        int highest_available_age = 0;
        for (int age = start_age; age <= end_age; age++) {
            try {
                // Test if this age exists in the table
                current_population_table.at(age);
                highest_available_age = age;
            } catch (const std::exception&) {
                // Age not found, keep the last highest age
            }
        }
        
        if (highest_available_age < start_age) {
            // Critical error - can't find any valid ages
            std::cerr << "CRITICAL ERROR: No valid ages found in population distribution table" << std::endl;
            throw std::runtime_error("No valid ages in population table");
        }
        
        // Process all ages in the age range
        for (int age = start_age; age <= end_age; age++) {
            try {
                // For ages within the available data, use the actual data
                if (age <= highest_available_age) {
                    const auto &age_info = current_population_table.at(age);
                    expected_population.at(age, core::Gender::male) = static_cast<int>(
                        std::round(age_info.males * start_population_size / total_initial_population));
                    expected_population.at(age, core::Gender::female) = static_cast<int>(
                        std::round(age_info.females * start_population_size / total_initial_population));
                } 
                // For ages beyond the available data, use the highest age data exactly as is
                else {
                    const auto &highest_age_info = current_population_table.at(highest_available_age);
                    
                    expected_population.at(age, core::Gender::male) = static_cast<int>(
                        std::round(highest_age_info.males * start_population_size / total_initial_population));
                    expected_population.at(age, core::Gender::female) = static_cast<int>(
                        std::round(highest_age_info.females * start_population_size / total_initial_population));
                }
            }
            catch (const std::exception& e) {
                std::cerr << "ERROR processing age " << age << " in population table: " << e.what() << std::endl;
                // Use neighboring age data if available or set to minimal values
                if (age > start_age) {
                    // Use previous age data if available
                    expected_population.at(age, core::Gender::male) = expected_population.at(age - 1, core::Gender::male) / 2;
                    expected_population.at(age, core::Gender::female) = expected_population.at(age - 1, core::Gender::female) / 2;
                } else {
                    // Minimal fallback
                    expected_population.at(age, core::Gender::male) = 1;
                    expected_population.at(age, core::Gender::female) = 1;
                }
            }
        }

        return expected_population;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR in get_current_expected_population: " << e.what() << std::endl;
        throw; // Re-throw to make sure the caller knows there was a critical error
    }
}

IntegerAgeGenderTable Simulation::get_current_simulated_population() {
    auto simulated_population = create_age_gender_table<int>(context_.age_range());
    auto &pop = context_.population();
    auto count_mutex = std::mutex{};
    tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        auto lock = std::unique_lock{count_mutex};
        simulated_population.at(entity.age, entity.gender)++;
    });

    return simulated_population;
}

void Simulation::apply_net_migration(int net_value, unsigned int age, const core::Gender &gender) {
    if (net_value > 0) {
        auto &pop = context_.population();
        auto similar_indices = core::find_index_of_all(pop, [&](const Person &entity) {
            return entity.is_active() && entity.age == age && entity.gender == gender;
        });

        if (!similar_indices.empty()) {
            // Needed for repeatability in random selection
            std::sort(similar_indices.begin(), similar_indices.end());

            // Safety check - don't try to add more than a reasonable number
            const int max_additions = 1000; // Set a reasonable limit
            int additions_to_make = std::min(net_value, max_additions);
            
            for (auto trial = 0; trial < additions_to_make; trial++) {
                // Ensure we don't have index out of bounds or overflow issues
                if (similar_indices.size() <= 1) {
                    // If only one element, use index 0
                    const auto &source = pop.at(similar_indices.at(0));
                    context_.population().add(partial_clone_entity(source), context_.time_now());
                } else {
                    // Cast to int safely and ensure bounds are respected
                    int max_index = static_cast<int>(similar_indices.size()) - 1;
                    if (max_index <= 0) max_index = 0;
                    
                    int index = context_.random().next_int(max_index);
                const auto &source = pop.at(similar_indices.at(index));
                context_.population().add(partial_clone_entity(source), context_.time_now());
                }
            }
        }
    } else if (net_value < 0) {
        // Ensure we don't remove more than a reasonable number
        const int max_removals = 1000; // Set a reasonable limit
        int net_value_counter = std::max(net_value, -max_removals);
        
        for (auto &entity : context_.population()) {
            if (!entity.is_active()) {
                continue;
            }

            if (entity.age == age && entity.gender == gender) {
                entity.emigrate(context_.time_now());
                net_value_counter++;
                if (net_value_counter == 0) {
                    break;
                }
            }
        }
    }
}

IntegerAgeGenderTable Simulation::get_net_migration() {
    // For baseline, create and return the migration data directly
    if (context_.scenario().type() == ScenarioType::baseline) {
        try {
            std::cout << "DEBUG: [get_net_migration] Baseline scenario - creating migration data" << std::endl;
            auto data = create_net_migration();
            std::cout << "DEBUG: [get_net_migration] Baseline scenario - created migration data successfully" << std::endl;
            return data;
        } catch (const std::exception& e) {
            std::cerr << "ERROR in baseline get_net_migration: " << e.what() << std::endl;
            // Create an empty fallback with zeros
            std::cout << "DEBUG: [get_net_migration] Baseline scenario - creating fallback migration data with zeros" << std::endl;
            auto fallback = create_age_gender_table<int>(context_.age_range());
            auto start_age = context_.age_range().lower();
            auto end_age = context_.age_range().upper();
            for (int age = start_age; age <= end_age; age++) {
                fallback.at(age, core::Gender::male) = 0;
                fallback.at(age, core::Gender::female) = 0;
            }
            return fallback;
        }
    }

    // For intervention, try to receive the migration data with significantly more patience
    std::cout << "DEBUG: [get_net_migration] Intervention scenario - attempting to receive migration data" << std::endl;
    
    // Create a default fallback table with zeros
    auto fallback_migration = create_age_gender_table<int>(context_.age_range());
    auto start_age = context_.age_range().lower();
    auto end_age = context_.age_range().upper();
    for (int age = start_age; age <= end_age; age++) {
        fallback_migration.at(age, core::Gender::male) = 0;
        fallback_migration.at(age, core::Gender::female) = 0;
    }
    
    try {
        // Calculate size factor for timeouts - increase significantly for larger populations
        // Make this much larger to be more patient
        double size_factor = std::min(40.0, std::max(5.0, static_cast<double>(context_.population().initial_size()) / 2000.0));
        
        // Try multiple times with increasing wait periods - maximum attempts increased
        int max_attempts = context_.population().initial_size() > 10000 ? 20 : 10;
        
        // First large wait to give baseline time to generate data
        int initial_pause = static_cast<int>(5000 * size_factor);
        std::cout << "DEBUG: [get_net_migration] Intervention waiting for " << initial_pause << "ms initially" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(initial_pause));
        
        for (int attempt = 0; attempt < max_attempts; attempt++) {
            // Exponential backoff with special case for first year and population size
            // Use significantly longer waits
            
            // Calculate backoff factor with overflow protection
            int backoff_factor = 1;
            if (attempt < 31) { // Prevent overflow with shift
                backoff_factor = (1 << std::min(attempt, 8));
            } else {
                backoff_factor = (1 << 8); // Maximum value
            }
            
            // Calculate wait time with overflow protection
            double raw_wait_time = 2000.0 * size_factor * backoff_factor;
            // Clamp to a reasonable maximum value to prevent overflow
            auto wait_time = static_cast<int>(std::min(raw_wait_time, 2.0 * 1000 * 1000)); // Max 2000 seconds
            
            // Special handling for first simulation year
            if (context_.time_now() == context_.start_time()) {
                if (attempt == 0) {
                    // Give baseline an even larger head start on the first simulation year
                    wait_time = static_cast<int>(std::min(15000.0 * size_factor, 2.0 * 1000 * 1000)); // Max 2000 seconds
                } else {
                    // For subsequent attempts in first year, use longer backoff
                    raw_wait_time = 4000.0 * size_factor * backoff_factor;
                    wait_time = static_cast<int>(std::min(raw_wait_time, 2.0 * 1000 * 1000)); // Max 2000 seconds
                }
            }
            
            // Skip the sleep on first attempt after the initial pause
            if (attempt > 0) {
                std::cout << "DEBUG: [get_net_migration] Intervention attempt " << attempt+1 << "/" << max_attempts 
                          << ", waiting " << wait_time << "ms" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            } else {
                std::cout << "DEBUG: [get_net_migration] Intervention attempt " << attempt+1 << "/" << max_attempts << std::endl;
            }
            
            try {
                // Try to receive the message with a timeout scaled by population size 
                // Triple the wait time for timeout, but limit to prevent overflow
                double raw_timeout = wait_time * 3.0;
                auto receive_timeout = static_cast<int>(std::min(raw_timeout, 2.0 * 1000 * 1000)); // Max 2000 seconds
                std::cout << "DEBUG: [get_net_migration] Attempting to receive with " << receive_timeout << "ms timeout" << std::endl;
                auto message = context_.scenario().channel().try_receive(receive_timeout);
                
                if (message.has_value()) {
                    std::cout << "DEBUG: [get_net_migration] Received a message!" << std::endl;
                    try {
                        auto &basePtr = message.value();
                        auto *messagePtr = dynamic_cast<NetImmigrationMessage *>(basePtr.get());
                        
                        if (messagePtr) {
                            try {
                                std::cout << "DEBUG: [get_net_migration] Message is NetImmigrationMessage type" << std::endl;
                                auto data = messagePtr->data();
                                
                                // Do a quick validation of the data
                                bool valid = true;
                                for (int age = start_age; age <= end_age && valid; age++) {
                                    valid = data.contains(age, core::Gender::male) && 
                                          data.contains(age, core::Gender::female);
                                }
                                
                                if (valid) {
                                    std::cout << "DEBUG: [get_net_migration] Successfully validated migration data" << std::endl;
                                    return data;
                                } else {
                                    std::cerr << "WARNING: Received migration data is incomplete" << std::endl;
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "ERROR accessing message data: " << e.what() << std::endl;
                            }
                        } else {
                            std::cerr << "WARNING: Received wrong message type on attempt " << (attempt + 1) << std::endl;
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "WARNING: Error processing received message: " << e.what() << std::endl;
                    }
                } else {
                    std::cout << "DEBUG: [get_net_migration] No message received after timeout" << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "WARNING: Receive attempt " << (attempt + 1) << " failed: " << e.what() << std::endl;
                // Continue to next attempt
            }
            
            // After a few failed attempts, check if we should retry baseline sim
            if (attempt == 5) {
                try {
                    // Try to generate our own migration data as a backup approach
                    std::cout << "DEBUG: [get_net_migration] Generating backup migration data after 5 failed attempts" << std::endl;
                    auto backup_data = create_net_migration();
                    
                    // Use this data if all further attempts fail
                    fallback_migration = backup_data;
                    std::cout << "DEBUG: [get_net_migration] Successfully generated backup data" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "ERROR generating backup migration data: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "DEBUG: [get_net_migration] All attempts failed, using fallback data" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR in get_net_migration: " << e.what() << std::endl;
    }
    
    // Always return something - either the zeros or our backup data
    return fallback_migration;
}

IntegerAgeGenderTable Simulation::create_net_migration() {
    std::cout << "DEBUG: [create_net_migration] Starting" << std::endl;
    
    // Create an empty table with zeros for all age/gender combinations
    auto net_emigration = create_age_gender_table<int>(context_.age_range());
    auto start_age = context_.age_range().lower();
    auto end_age = context_.age_range().upper();
    
    // Initialize all cells to zero
    for (int age = start_age; age <= end_age; age++) {
        net_emigration.at(age, core::Gender::male) = 0;
        net_emigration.at(age, core::Gender::female) = 0;
    }
    
    try {
        std::cout << "DEBUG: [create_net_migration] Getting simulated population" << std::endl;
        
        // Get simulated population with timeout protection
        IntegerAgeGenderTable simulated_population;
        auto sim_pop_future = std::async(std::launch::async, [this]() { 
            return this->get_current_simulated_population(); 
        });
        
        if (sim_pop_future.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
            std::cerr << "ERROR: Timeout while getting simulated population data" << std::endl;
            return net_emigration; // Return zeros on timeout
        }
        
        simulated_population = sim_pop_future.get();
        std::cout << "DEBUG: [create_net_migration] Successfully retrieved simulated population" << std::endl;
        
        // Only proceed with expected population if simulated population is valid
        bool simulated_valid = true;
        for (int age = start_age; age <= end_age && simulated_valid; age++) {
            simulated_valid = simulated_population.contains(age, core::Gender::male) && 
                            simulated_population.contains(age, core::Gender::female);
        }
        
        if (!simulated_valid) {
            std::cerr << "WARNING: Simulated population data is incomplete. Using zeros for migration." << std::endl;
            return net_emigration; // Return zeros
        }
        
        // Get expected population only after verifying simulated is valid
        std::cout << "DEBUG: [create_net_migration] Getting expected population" << std::endl;
        IntegerAgeGenderTable expected_population;
        try {
            auto expected_future = std::async(std::launch::async, [this]() { 
                return this->get_current_expected_population(); 
            });
            
            if (expected_future.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
                std::cerr << "ERROR: Timeout while getting expected population data" << std::endl;
                return net_emigration; // Return zeros on timeout
            }
            
                expected_population = expected_future.get();
            std::cout << "DEBUG: [create_net_migration] Successfully retrieved expected population" << std::endl;
        }
        catch(const std::exception& e) {
            std::cerr << "ERROR getting expected population: " << e.what() << std::endl;
            return net_emigration; // Return zeros on error
        }
        
        // Verify expected population is valid
        bool expected_valid = true;
        for (int age = start_age; age <= end_age && expected_valid; age++) {
            expected_valid = expected_population.contains(age, core::Gender::male) && 
                           expected_population.contains(age, core::Gender::female);
        }
        
        if (!expected_valid) {
            std::cerr << "WARNING: Expected population data is incomplete. Using zeros for migration." << std::endl;
            return net_emigration; // Return zeros
        }
        
        std::cout << "DEBUG: [create_net_migration] Calculating migration differences" << std::endl;
        
        // Both populations are verified valid, now calculate migration
        for (int age = start_age; age <= end_age; age++) {
            try {
                // Males - we've verified contains() already, but add extra safety
                if (expected_population.contains(age, core::Gender::male) && 
                    simulated_population.contains(age, core::Gender::male)) {
                    
                    int expected_males = expected_population.at(age, core::Gender::male);
                    int simulated_males = simulated_population.at(age, core::Gender::male);
                    
                    // Calculate net migration
                    net_emigration.at(age, core::Gender::male) = expected_males - simulated_males;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error calculating male migration for age " << age << ": " << e.what() << std::endl;
                // Keep value as 0
            }
            
            try {
                // Females - with extra safety
                if (expected_population.contains(age, core::Gender::female) && 
                    simulated_population.contains(age, core::Gender::female)) {
                    
                    int expected_females = expected_population.at(age, core::Gender::female);
                    int simulated_females = simulated_population.at(age, core::Gender::female);
                    
                    // Calculate net migration
                    net_emigration.at(age, core::Gender::female) = expected_females - simulated_females;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error calculating female migration for age " << age << ": " << e.what() << std::endl;
                // Keep value as 0
            }
        }
        
        std::cout << "DEBUG: [create_net_migration] Migration calculation complete" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error in create_net_migration: " << e.what() << std::endl;
    }

    return net_emigration;
}

Person Simulation::partial_clone_entity(const Person &source) noexcept {
    auto clone = Person{};
    clone.age = source.age;
    clone.gender = source.gender;
    clone.ses = source.ses;
    clone.sector = source.sector;
    clone.region = source.region;                   // added region for FINCH
    clone.income_category = source.income_category; // added income_category for FINCH
    clone.ethnicity = source.ethnicity;             // added ethnicity for FINCH
    for (const auto &item : source.risk_factors) {
        clone.risk_factors.emplace(item.first, item.second);
    }

    for (const auto &item : source.diseases) {
        clone.diseases.emplace(item.first, item.second.clone());
    }

    return clone;
}

std::map<std::string, core::UnivariateSummary> Simulation::create_input_data_summary() const {
    auto visitor = UnivariateVisitor();
    auto summary = std::map<std::string, core::UnivariateSummary>();
    const auto &input_data = context_.inputs().data();

    for (const auto &entry : context_.mapping()) {
        // HACK: Ignore missing columns.
        if (const auto &column = input_data.column_if_exists(entry.name())) {
            column->get().accept(visitor);
            summary.emplace(entry.name(), visitor.get_summary());
        }
    }

    return summary;
}

void hgps::Simulation::print_initial_population_statistics() {
    auto verbosity = context_.inputs().run().verbosity;
    if (context_.current_run() > 1 && verbosity == core::VerboseMode::none) {
        return;
    }

    auto original_future = core::run_async(&Simulation::create_input_data_summary, this);
    std::string population = "Population";
    std::size_t longestColumnName = population.length();
    auto sim_summary = std::map<std::string, core::UnivariateSummary>();
    for (const auto &entry : context_.mapping()) {
        longestColumnName = std::max(longestColumnName, entry.name().length());
        sim_summary.emplace(entry.name(), core::UnivariateSummary(entry.name()));
    }

    for (const auto &entity : context_.population()) {
        for (const auto &entry : context_.mapping()) {
            sim_summary[entry.name()].append(entity.get_risk_factor_value(entry.key()));
        }
    }

    auto pad = longestColumnName + 2;
    auto width = pad + 70;
    auto orig_pop = context_.inputs().data().num_rows();
    auto sim_pop = context_.population().size();

    std::stringstream ss;
    ss << fmt::format("\n Initial Virtual Population Summary: {}\n", context_.identifier());
    ss << fmt::format("|{:-<{}}|\n", '-', width);
    ss << fmt::format("| {:{}} : {:>14} : {:>14} : {:>14} : {:>14} |\n", "Variable", pad,
                      "Mean (Real)", "Mean (Sim)", "StdDev (Real)", "StdDev (Sim)");
    ss << fmt::format("|{:-<{}}|\n", '-', width);

    ss << fmt::format("| {:{}} : {:14} : {:14} : {:14} : {:14} |\n", population, pad, orig_pop,
                      sim_pop, orig_pop, sim_pop);

    auto orig_summary = original_future.get();
    for (const auto &entry : context_.mapping()) {
        const auto &col = entry.name();
        ss << fmt::format("| {:{}} : {:14.4f} : {:14.5f} : {:14.5f} : {:14.5f} |\n", col, pad,
                          orig_summary[col].average(), sim_summary[col].average(),
                          orig_summary[col].std_deviation(), sim_summary[col].std_deviation());
    }

    ss << fmt::format("|{:_<{}}|\n\n", '_', width);
    std::cout << ss.str();
}

// Write person data to a stream
// This function writes the person data to a stream in a CSV format
// because the person data is stored in an unordered_map, we need to iterate through the map and
// write the data to the stream
void Simulation::write_person_data(std::ostream &stream, const Person &person,
                                   const unsigned int time) const {
    stream << time << separator_ << person.id() << separator_ << person.age << separator_
           << person.gender_to_string() << separator_ << person.sector_to_value() << separator_
           << person.income_continuous << separator_ // Add continuous income
           << person.income_to_value() << separator_ // This now returns income_category value
           << person.region_to_value() << separator_ << person.ethnicity_to_value();

    // Write risk factors
    for (const auto &name : risk_factor_names_) {
        stream << separator_;
        if (person.risk_factors.contains(name)) {
            stream << person.risk_factors.at(name);
        } else {
            stream << "NA";
        }
    }
    stream << '\n';
}

void Simulation::write_header(std::ostream &stream) const {
    stream << "Time" << separator_ << "ID" << separator_ << "Age" << separator_ << "Gender"
           << separator_ << "Sector" << separator_ << "Income_Continuous"
           << separator_                      // Add continuous income header
           << "Income_Category" << separator_ // Rename to be explicit
           << "Region" << separator_ << "Ethnicity";

    // Write risk factor names
    for (const auto &name : risk_factor_names_) {
        stream << separator_ << name.to_string();
    }
    stream << '\n';
}

} // namespace hgps
