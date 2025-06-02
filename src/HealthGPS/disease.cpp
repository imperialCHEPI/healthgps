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

    // Update blood pressure medication and systolic blood pressure after diseases are initialized
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        update_blood_pressure_medication(person, context.random());
        update_systolic_blood_pressure(person, context.random());
    }
    std::cout
        << "\nDEBUG: Completed updating blood pressure medication and systolic blood pressure";
}

void DiseaseModule::update_population(RuntimeContext &context) {
    for (auto &model : models_) {
        model.second->update_disease_status(context);
    }

    // Update blood pressure medication and systolic blood pressure after diseases are updated
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }
        update_blood_pressure_medication(person, context.random());
        update_systolic_blood_pressure(person, context.random());
    }
    std::cout
        << "\nDEBUG: Completed updating blood pressure medication and systolic blood pressure";
}

double DiseaseModule::get_excess_mortality(const core::Identifier &disease_code,
                                           const Person &entity) const noexcept {
    if (!models_.contains(disease_code)) {
        return 0.0;
    }

    return models_.at(disease_code)->get_excess_mortality(entity);
}

void DiseaseModule::update_blood_pressure_medication(Person &person, Random &random) const {
    // If person is already on medication, they stay on it
    if (person.risk_factors.contains("BloodPressureMedication"_id) &&
        person.risk_factors.at("BloodPressureMedication"_id) == 1.0) {
        return;
    }

    // Get current SBP and BMI
    double sbp = person.risk_factors.at("SystolicBloodPressure"_id);
    double bmi = person.risk_factors.at("BMI"_id);

    // Check if person has diabetes
    bool has_diabetes = person.diseases.contains("diabetes"_id) &&
                        person.diseases.at("diabetes"_id).status == DiseaseStatus::active;

    // Start medication if:
    // 1. SBP >= 160 OR
    // 2. SBP >= 140 AND BMI >= 25 AND has diabetes
    if (sbp >= 160.0 || (sbp >= 140.0 && bmi >= 25.0 && has_diabetes)) {
        person.risk_factors["BloodPressureMedication"_id] = 1.0;
    } else {
        person.risk_factors["BloodPressureMedication"_id] = 0.0;
    }
}

void DiseaseModule::update_systolic_blood_pressure(Person &person, Random &random) const {
    // Get the systolic blood pressure model parameters
    auto model_it = systolic_blood_pressure_models_.find(core::Identifier("systolicbloodpressure"));
    if (model_it == systolic_blood_pressure_models_.end()) {
        std::cout << "\nWARNING: Systolic blood pressure model not found, skipping update";
        return;
    }

    const auto &model = model_it->second;

    // Calculate the linear predictor
    double predictor = model.intercept;

    // Add gender effect
    if (person.gender == core::Gender::male) {
        predictor += model.coefficients.at("Gender");
    }

    // Add age effects
    predictor += model.coefficients.at("Age") * person.age;
    predictor += model.coefficients.at("Age2") * person.age * person.age;
    predictor += model.coefficients.at("Age3") * person.age * person.age * person.age;

    // Add BMI effect
    double bmi = person.risk_factors.at("BMI"_id);
    predictor += model.coefficients.at("BMI") * bmi;

    // Add blood pressure medication effect if available
    if (person.risk_factors.contains("BloodPressureMedication"_id)) {
        double medication = person.risk_factors.at("BloodPressureMedication"_id);
        predictor += model.coefficients.at("BloodPressureMedication") * medication;
    }

    // Add random noise and store it as residual for longitudinal correlation
    double stddev = model.coefficients.at("stddev");
    double residual = random.next_normal(0.0, stddev);
    person.sbp_residual = residual;

    // Calculate final SBP value
    double sbp = predictor + residual;

    // Clamp value between min and max if specified
    if (model.coefficients.contains("min") && model.coefficients.contains("max")) {
        sbp = std::clamp(sbp, model.coefficients.at("min"), model.coefficients.at("max"));
    }

    // Set the systolic blood pressure value
    person.risk_factors["SystolicBloodPressure"_id] = sbp;
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

    // Load systolic blood pressure and blood pressure medication models
    auto module = std::make_unique<DiseaseModule>(std::move(models));
    module->systolic_blood_pressure_models_ = repository.get_systolic_blood_pressure_models();
    module->blood_pressure_medication_models_ = repository.get_blood_pressure_medication_models();
    return module;
}
} // namespace hgps
