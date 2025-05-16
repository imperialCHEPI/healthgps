#include "disease.h"
#include "disease_registry.h"
#include "lms_model.h"
#include "weight_model.h"

#include <oneapi/tbb/parallel_for_each.h>

namespace hgps {

DiseaseModule::DiseaseModule(std::map<core::Identifier, std::shared_ptr<DiseaseModel>> &&models)
    : models_{std::move(models)} {}

SimulationModuleType DiseaseModule::type() const noexcept { return SimulationModuleType::Disease; }

const std::string &DiseaseModule::name() const noexcept { return name_; }

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

void DiseaseModule::initialise_population(RuntimeContext &context) {
    // Initialise disease status based on prevalence
    for (auto &model : models_) {
        // std::cout << "\nDEBUG: Initializing disease status for: " << model.first.to_string() << "
        // (" << ++disease_count << "/" << models_.size() << ")";
        model.second->initialise_disease_status(context);
    }
    std::cout << "\nDEBUG: Completed initializing disease status for all diseases";

    // Recalculate relative risks once diseases status were generated
    for (auto &model : models_) {
        // std::cout << "\nDEBUG: Initializing average relative risk for: " <<
        // model.first.to_string() << " (" << ++disease_count << "/" << models_.size() << ")";
        model.second->initialise_average_relative_risk(context);
    }
    std::cout << "\nDEBUG: Completed initializing average relative risk for all diseases";

    // After initialising with prevalence, do a 'dry run' to simulate incidence.
    for (auto &model : models_) {
        // std::cout << "\nDEBUG: Updating disease status (dry run) for: " <<
        // model.first.to_string() << " (" << ++disease_count << "/" << models_.size() << ")";
        model.second->update_disease_status(context);
        std::cout << "\nDEBUG: Finished update for: " << model.first.to_string();
    }
    std::cout << "\nDEBUG: Completed updating disease status (dry run) for all diseases";
}

void DiseaseModule::update_population(RuntimeContext &context) {
    for (auto &model : models_) {
        model.second->update_disease_status(context);
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
