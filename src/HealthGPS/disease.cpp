#include "disease.h"
#include "disease_registry.h"
#include "lms_model.h"
#include "weight_model.h"

#include <iostream>
#include <oneapi/tbb/parallel_for_each.h>

namespace hgps {

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
    for (auto &model : models_) {
        // Special handling for gallbladder disease
        if (model.first.to_string() == "gallbladder") {
            std::cout << "DEBUG: [DiseaseModule] Skipping gallbladder disease (using dummy data)"
                      << std::endl;
            continue;
        }

        try {
            // Add detailed error handling around disease updates
            std::cout << "DEBUG: [DiseaseModule] Updating " << model.first.to_string()
                      << " disease status" << std::endl;
            model.second->update_disease_status(context);
        } catch (const std::out_of_range &e) {
            // Handle "invalid map<K, T> key" error specifically
            std::cerr << "ERROR: Map key error while updating " << model.first.to_string()
                      << " disease status: " << e.what() << std::endl;
        } catch (const std::exception &e) {
            // Handle other exceptions
            std::cerr << "ERROR: Exception while updating " << model.first.to_string()
                      << " disease status: " << e.what() << std::endl;
        } catch (...) {
            // Catch all unexpected errors
            std::cerr << "ERROR: Unknown error while updating " << model.first.to_string()
                      << " disease status" << std::endl;
        }
    }
}

double DiseaseModule::get_excess_mortality(const core::Identifier &disease_code,
                                           const Person &entity) const noexcept {
    if (!models_.contains(disease_code)) {
        return 0.0;
    }

    return models_.at(disease_code)->get_excess_mortality(entity);
}

std::unique_ptr<DiseaseModule> build_disease_module(Repository &repository,
                                                    const ModelInput &config) {
    // Models must be registered prior to be created.
    auto registry = get_default_disease_model_registry();
    auto models = std::map<core::Identifier, std::shared_ptr<DiseaseModel>>();

    const auto &diseases = config.diseases();
    std::mutex m;
    tbb::parallel_for_each(std::begin(diseases), std::end(diseases), [&](auto &info) {
        auto info_code_str = info.code.to_string();
        auto other = repository.get_disease_info(info_code_str);
        if (!registry.contains(info.group) || !other.has_value()) {
            throw std::out_of_range("Unknown disease definition: " + info_code_str);
        }

        auto &disease_definition = repository.get_disease_definition(info, config);
        auto &lms_definition = repository.get_lms_definition();
        auto classifier = WeightModel{LmsModel{lms_definition}};

        // Sync region
        std::scoped_lock lock(m);
        models.emplace(core::Identifier{info.code},
                       registry.at(info.group)(disease_definition, std::move(classifier),
                                               config.settings().age_range()));
    });

    return std::make_unique<DiseaseModule>(std::move(models));
}
} // namespace hgps
