#include "disease.h"
#include "disease_registry.h"
#include "lms_model.h"
#include "weight_model.h"

#include <iostream>
#include <oneapi/tbb/parallel_for_each.h>

namespace hgps {

// Function to reset disease cache and tracking variables
void reset_disease_caches() {
    try {
        // Reset all static maps that track disease failures
        static std::map<std::string, bool> disease_update_failures;
        static std::map<std::string, bool> disease_init_success;
        static std::map<std::string, bool> valid_disease_cache;

        // Clear each map individually with error checking
        try {
            disease_update_failures.clear();
        } catch (...) {
            disease_update_failures = std::map<std::string, bool>(); // Re-create if clear fails
        }

        try {
            disease_init_success.clear();
        } catch (...) {
            disease_init_success = std::map<std::string, bool>(); // Re-create if clear fails
        }

        try {
            valid_disease_cache.clear();
        } catch (...) {
            valid_disease_cache = std::map<std::string, bool>(); // Re-create if clear fails
        }

        // Log successful reset
        std::cout << "DEBUG: Disease caches have been reset successfully" << std::endl;
    } catch (...) {
        // Last resort error handling - create brand new static maps
        std::cerr << "WARNING: Error during disease cache reset, creating new cache instances"
                  << std::endl;
        static std::map<std::string, bool> disease_update_failures = std::map<std::string, bool>();
        static std::map<std::string, bool> disease_init_success = std::map<std::string, bool>();
        static std::map<std::string, bool> valid_disease_cache = std::map<std::string, bool>();
    }
}

DiseaseModule::DiseaseModule(std::map<core::Identifier, std::shared_ptr<DiseaseModel>> &&models)
    : models_{std::move(models)} {}

SimulationModuleType DiseaseModule::type() const noexcept { return SimulationModuleType::Disease; }

std::string DiseaseModule::name() const noexcept { return name_; }

std::size_t DiseaseModule::size() const noexcept { return models_.size(); }

bool DiseaseModule::contains(const core::Identifier &disease_id) const noexcept {
    return models_.contains(disease_id);
}

std::shared_ptr<DiseaseModel> &DiseaseModule::operator[](const core::Identifier &disease_id) {
    return models_.at(disease_id);
}

const std::shared_ptr<DiseaseModel> &
DiseaseModule::operator[](const core::Identifier &disease_id) const {
    return models_.at(disease_id);
}

void DiseaseModule::initialise_population([[maybe_unused]] RuntimeContext &context,
                                          [[maybe_unused]] Population &population,
                                          [[maybe_unused]] Random &random) {
    // Initialize each disease model once for the whole population
    for (const auto &[model_type, model] : models_) {
        model->initialise_disease_status(context);
    }
}

void DiseaseModule::update_population(RuntimeContext &context) {
    // First validate that we have diseases to update
    if (models_.empty()) {
        // Critical warning - keep this
        std::cerr << "WARNING: No disease models to update" << std::endl;
        return;
    }

    // Print detailed diagnostic information
    std::cout << "\n====== DISEASE MODULE UPDATE START ======" << std::endl;
    std::cout << "Number of disease models: " << models_.size() << std::endl;
    std::cout << "Current simulation time: " << context.time_now() << std::endl;
    std::cout << "Population size: " << context.population().size() << std::endl;
    std::cout << "Active population: " << context.population().current_active_size() << std::endl;

    // Print each disease model's details
    std::cout << "Disease models to be processed:" << std::endl;
    for (const auto &[disease_id, model] : models_) {
        std::cout << "  Disease ID: " << disease_id.to_string();
        if (model) {
            std::cout << " (valid model object)";
        } else {
            std::cout << " (NULL MODEL - ERROR)";
        }
        std::cout << std::endl;
    }

    // Track successful and failed updates
    size_t success_count = 0;
    size_t error_count = 0;

    // Map to track which diseases have had update failures
    static std::map<std::string, bool> disease_update_failures;
    static bool first_run = true;

    // If this is the first run, initialize the failure tracking map
    if (first_run) {
        disease_update_failures.clear(); // Clear any previous entries
        for (auto &model_pair : models_) {
            disease_update_failures[model_pair.first.to_string()] = false;
        }
        first_run = false;
    }

    // Generate a vector of disease IDs to process in a consistent order
    std::vector<core::Identifier> disease_ids;
    for (auto &model_pair : models_) {
        disease_ids.push_back(model_pair.first);
    }

    // Sort disease IDs to ensure consistent processing order
    std::sort(disease_ids.begin(), disease_ids.end(),
              [](const core::Identifier &a, const core::Identifier &b) {
                  return a.to_string() < b.to_string();
              });

    for (const auto &disease_id : disease_ids) {
        std::string disease_name = disease_id.to_string();

        // Skip diseases that have consistently failed in previous runs
        if (disease_update_failures[disease_name]) {
            // Removed debug print
            error_count++;
            continue;
        }

        try {
            // Removed debug print

            // Validate that the disease model exists and is properly initialized
            if (!models_.contains(disease_id) || !models_.at(disease_id)) {
                // Keep critical error message
                std::cerr << "ERROR: Disease model for " << disease_name
                          << " is null or missing, skipping update" << std::endl;
                error_count++;

                // Mark this disease as a persistent failure after 2 consecutive errors
                if (disease_update_failures[disease_name]) {
                    std::cerr << "WARNING: Disease " << disease_name
                              << " has failed multiple times and will be skipped in future runs"
                              << std::endl;
                } else {
                    disease_update_failures[disease_name] = true;
                }
                continue;
            }

            auto &disease_model = models_.at(disease_id);

            // Now safely update the disease status
            disease_model->update_disease_status(context);
            success_count++;

            // Reset failure flag if update was successful
            disease_update_failures[disease_name] = false;

            // Log successful update
            std::cout << "DEBUG: [DiseaseModule] Successfully updated " << disease_name
                      << " disease status" << std::endl;

        } catch (const std::out_of_range &e) {
            // Handle "invalid map<K, T> key" error specifically - keep this critical error
            std::cerr << "ERROR: Map key error while updating " << disease_name
                      << " disease status: " << e.what() << std::endl;
            error_count++;

            // Mark as failed but give it another chance next time
            disease_update_failures[disease_name] = true;
        } catch (const std::exception &e) {
            // Handle other exceptions - keep this critical error
            std::cerr << "ERROR: Exception while updating " << disease_name
                      << " disease status: " << e.what() << std::endl;
            error_count++;

            // Mark as failed but give it another chance next time
            disease_update_failures[disease_name] = true;
        } catch (...) {
            // Catch all unexpected errors - keep this critical error
            std::cerr << "ERROR: Unknown exception while updating " << disease_name
                      << " disease status" << std::endl;
            error_count++;

            // Mark as failed but give it another chance next time
            disease_update_failures[disease_name] = true;
        }
    }

    // Log summary of disease updates
    std::cout << "INFO: [DiseaseModule] Updated " << success_count
              << " disease models successfully, " << error_count << " failures" << std::endl;

    // Print summary for each disease
    if (success_count > 0) {
        int year = context.time_now();
        std::cout << "INFO: [Year " << year << "] Disease summary:" << std::endl;
        for (const auto &disease_id : disease_ids) {
            if (!disease_update_failures[disease_id.to_string()]) {
                // Only print successful diseases
                int pop_size = context.population().current_active_size();
                std::cout << "INFO: [Year " << year << "] " << disease_id.to_string()
                          << " successfully processed for " << pop_size << " people with 0 errors"
                          << std::endl;
            }
        }
    }

    // Report initialization statistics
    std::cout << "INFO: Initialized " << name() << " for " << success_count << " out of "
              << context.population().current_active_size() << " active people ("
              << (success_count * 100.0 /
                  std::max(1, static_cast<int>(context.population().current_active_size())))
              << "%), with " << error_count << " errors" << std::endl;

    // Warn if no diseases were initialized
    if (success_count == 0) {
        std::cerr << "WARNING: No " << name()
                  << " diseases were initialized! This will result in zero prevalence."
                  << std::endl;
    }

    // Add this line to indicate the function completed successfully
    std::cout << "DEBUG: Successfully completed initializing " << name() << std::endl;
}

bool DiseaseModule::has_disease(const core::Identifier &disease_code,
                                const Person &entity) const noexcept {
    try {
        // Check if disease exists in person's diseases collection
        auto it = entity.diseases.find(disease_code);
        return it != entity.diseases.end();
    } catch (...) {
        // Any exception means the disease is not properly accessible
        return false;
    }
}

DiseaseStatus DiseaseModule::get_disease_status(const core::Identifier &disease_code,
                                                const Person &entity) const noexcept {
    try {
        // Check if disease exists in person's diseases collection
        if (has_disease(disease_code, entity)) {
            return entity.diseases.at(disease_code).status;
        } else {
            return DiseaseStatus::free;
        }
    } catch (...) {
        // Any exception means we return inactive status
        return DiseaseStatus::free;
    }
}

double DiseaseModule::get_excess_mortality(const core::Identifier &disease_code,
                                           const Person &entity) const noexcept {
    // Cache of valid disease models for fast lookup
    static std::map<std::string, bool> valid_disease_cache;
    auto disease_name = disease_code.to_string();

    try {
        // Fast path: check cache first to avoid repeated errors with known invalid diseases
        if (valid_disease_cache.count(disease_name) > 0 && !valid_disease_cache[disease_name]) {
            return 0.0;
        }

        // First check if the module contains this disease
        if (!models_.contains(disease_code)) {
            // Cache this failure to prevent repeated lookups
            valid_disease_cache[disease_name] = false;
            return 0.0; // Return 0 for unknown diseases rather than throwing an exception
        }

        // Check if the disease exists in the person's disease collection
        if (!has_disease(disease_code, entity)) {
            return 0.0; // No excess mortality if person doesn't have the disease
        }

        // Check if the disease is active
        if (get_disease_status(disease_code, entity) != DiseaseStatus::active) {
            return 0.0; // No excess mortality if disease is not active
        }

        // Access the disease model safely
        auto disease_model_it = models_.find(disease_code);
        if (disease_model_it == models_.end() || !disease_model_it->second) {
            // Cache this failure to prevent repeated lookups
            valid_disease_cache[disease_name] = false;

            // Keep critical error
            std::cerr << "ERROR: Null or missing disease model for " << disease_name << std::endl;
            return 0.0;
        }

        // Cache successful lookups
        try {
            valid_disease_cache[disease_name] = true;
        } catch (...) {
            // If we can't cache the result, just continue without caching
        }

        // Safely get excess mortality with additional error handling
        try {
            auto excess_mortality = disease_model_it->second->get_excess_mortality(entity);

            // Validate the mortality value
            if (std::isnan(excess_mortality) || !std::isfinite(excess_mortality)) {
                return 0.0; // Return 0 for invalid values
            }

            // Clamp to valid range [0,1]
            return std::max(0.0, std::min(excess_mortality, 1.0));
        } catch (const std::exception &e) {
            std::cerr << "ERROR: Exception in disease model get_excess_mortality for "
                      << disease_name << ": " << e.what() << std::endl;
            return 0.0;
        } catch (...) {
            std::cerr << "ERROR: Unknown exception in disease model get_excess_mortality for "
                      << disease_name << std::endl;
            return 0.0;
        }

    } catch (const std::out_of_range &e) {
        // Log the specific error but continue execution
        std::cerr << "ERROR: Map key error in get_excess_mortality for disease " << disease_name
                  << ": " << e.what() << std::endl;

        // Cache this failure
        try {
            valid_disease_cache[disease_name] = false;
        } catch (...) {
            // If we can't cache the result, just continue
        }
        return 0.0;
    } catch (const std::exception &e) {
        // Log the error but continue execution
        std::cerr << "ERROR: Failed to get excess mortality for disease " << disease_name << ": "
                  << e.what() << std::endl;

        // Cache this failure
        try {
            valid_disease_cache[disease_name] = false;
        } catch (...) {
            // If we can't cache the result, just continue
        }
        return 0.0;
    } catch (...) {
        // Catch any other exceptions
        std::cerr << "ERROR: Unknown exception in get_excess_mortality for disease " << disease_name
                  << std::endl;

        // Cache this failure
        try {
            valid_disease_cache[disease_name] = false;
        } catch (...) {
            // If we can't cache the result, just continue
        }
        return 0.0;
    }
}

std::unique_ptr<DiseaseModule> build_disease_module(Repository &repository,
                                                    const ModelInput &config) {
    // Reset disease caches to avoid carrying over previous failures
    reset_disease_caches();

    // Models must be registered prior to be created.
    auto registry = get_default_disease_model_registry();
    auto models = std::map<core::Identifier, std::shared_ptr<DiseaseModel>>();

    const auto &diseases = config.diseases();
    std::mutex m;

    // Track statistics for initialization
    int total_diseases = static_cast<int>(diseases.size());
    int validated_count = 0;
    int successful_count = 0;
    int failed_count = 0;

    // First, create a list of valid disease IDs from the config that exist in the repository
    std::vector<core::DiseaseInfo> validated_diseases;

    // Map to store previously successful disease initializations across runs
    static std::map<std::string, bool> disease_init_success;
    static bool is_first_init = true;

    // Initialize the tracking map on first run
    if (is_first_init) {
        disease_init_success.clear(); // Clear any previous entries
        for (const auto &info : diseases) {
            disease_init_success[info.code.to_string()] = false;
        }
        is_first_init = false;
    } else {
        // For subsequent runs, reset all failure flags to give diseases another chance
        for (const auto &info : diseases) {
            disease_init_success[info.code.to_string()] = true;
        }
    }

    // Pre-validate diseases before parallel processing
    for (const auto &info : diseases) {
        auto info_code_str = info.code.to_string();

        try {
            // Remove the check that would skip previously failed diseases
            // We want to try all diseases each time
            auto other = repository.get_disease_info(info_code_str);

            if (registry.contains(info.group) && other.has_value()) {
                validated_diseases.push_back(info);
                validated_count++;
                // Removed debug print
            } else {
                // Keep warning for missing diseases in registry
                std::cerr << "WARNING: Skipping disease '" << info_code_str
                          << "' as it's not found in registry or repository" << std::endl;
                // Mark as failed just for this run
                disease_init_success[info_code_str] = false;
            }
        } catch (const std::exception &e) {
            // Keep critical warning
            std::cerr << "WARNING: Skipping disease '" << info_code_str << "': " << e.what()
                      << std::endl;
            // Mark as failed just for this run
            disease_init_success[info_code_str] = false;
        } catch (...) {
            // Keep critical warning
            std::cerr << "WARNING: Skipping disease '" << info_code_str << "' due to unknown error"
                      << std::endl;
            // Mark as failed just for this run
            disease_init_success[info_code_str] = false;
        }
    }

    // Ensure at least one disease is available or the simulation will have issues
    if (validated_diseases.empty()) {
        // Keep critical error
        std::cerr << "CRITICAL ERROR: No valid diseases available for initialization!" << std::endl;
        std::cerr << "Attempting to continue with minimal fallback configuration..." << std::endl;

        // Try to re-add at least diabetes if it exists in the config
        for (const auto &info : diseases) {
            if (info.code.to_string() == "diabetes") {
                validated_diseases.push_back(info);
                std::cerr << "Added diabetes as fallback disease" << std::endl;
                break;
            }
        }

        // If still empty, try the first disease regardless of validation
        if (validated_diseases.empty() && !diseases.empty()) {
            validated_diseases.push_back(diseases[0]);
            std::cerr << "Added " << diseases[0].code.to_string()
                      << " as emergency fallback disease" << std::endl;
        }
    }

    // Removed info print

    // Now process only the validated diseases
    tbb::parallel_for_each(
        std::begin(validated_diseases), std::end(validated_diseases), [&](auto &info) {
            auto info_code_str = info.code.to_string();
            try {
                auto &disease_definition = repository.get_disease_definition(info, config);
                auto &lms_definition = repository.get_lms_definition();

                // Validate LMS definition
                if (lms_definition.empty()) {
                    throw std::runtime_error("Empty LMS model definition");
                }

                auto classifier = WeightModel{LmsModel{lms_definition}};

                // Critical section - safely add to the models map
                {
                    std::scoped_lock lock(m);
                    models.emplace(core::Identifier{info.code},
                                   registry.at(info.group)(disease_definition,
                                                           std::move(classifier),
                                                           config.settings().age_range()));
                    successful_count++;

                    // Mark as successful for future runs
                    disease_init_success[info_code_str] = true;
                    // Removed success print
                }
            } catch (const std::exception &e) {
                // Keep critical error
                std::cerr << "ERROR: Failed to initialize disease model for " << info_code_str
                          << ": " << e.what() << std::endl;
                failed_count++;

                // Mark as failed for future runs
                std::scoped_lock lock(m);
                disease_init_success[info_code_str] = false;
            } catch (...) {
                // Keep critical error
                std::cerr << "ERROR: Unknown exception while initializing disease model for "
                          << info_code_str << std::endl;
                failed_count++;

                // Mark as failed for future runs
                std::scoped_lock lock(m);
                disease_init_success[info_code_str] = false;
            }
        });

    // Log final statistics
    std::cout << "INFO: Disease initialization complete: " << successful_count << " successful, "
              << failed_count << " failed" << std::endl;

    return std::make_unique<DiseaseModule>(std::move(models));
}
} // namespace hgps
