#include "static_linear_model.h"
#include "runtime_context.h"

#include "HealthGPS.Core/exception.h"

namespace hgps {

StaticLinearModel::StaticLinearModel(std::vector<core::Identifier> risk_factor_names,
                                     LinearModelParams risk_factor_models,
                                     Eigen::MatrixXd risk_factor_cholesky)
    : risk_factor_names_{std::move(risk_factor_names)},
      risk_factor_models_{std::move(risk_factor_models)},
      risk_factor_cholesky_{std::move(risk_factor_cholesky)} {

    if (risk_factor_names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (risk_factor_models_.intercepts.empty() || risk_factor_models_.coefficients.empty()) {
        throw core::HgpsException("Risk factor models mapping is incomplete");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
}

HierarchicalModelType StaticLinearModel::type() const noexcept {
    return HierarchicalModelType::Static;
}

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    // Approximate risk factor values with linear models.
    for (auto &person : context.population()) {
        linear_approximation(person);
    }

    // TODO: Correlated risk factor sampling.
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {

    for (auto &person : context.population()) {
        // Only newborns should be initialised.
        if (person.age > 0) {
            continue;
        }
    }
}

void StaticLinearModel::linear_approximation(Person &person) {

    // Approximate risk factor values for person with linear models.
    for (const auto &factor_name : risk_factor_names_) {
        double factor = risk_factor_models_.intercepts.at(factor_name);
        const auto &coefficients = risk_factor_models_.coefficients.at(factor_name);

        for (const auto &[coefficient_name, coefficient_value] : coefficients) {
            factor += coefficient_value * person.get_risk_factor_value(coefficient_name);
        }

        person.risk_factors[factor_name] = factor;
    }
}

std::unordered_map<core::Identifier, std::unordered_map<core::Gender, std::vector<double>>>
StaticLinearModel::mean_by_age_and_sex(RuntimeContext &context) {
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, std::vector<double>>>
        mean;
    const int max_age = context.age_range().upper() + 1;

    for (const auto &factor_name : risk_factor_names_) {
        mean[factor_name] = std::unordered_map<core::Gender, std::vector<double>>{};
        mean[factor_name].emplace(core::Gender::male, max_age);
        mean[factor_name].emplace(core::Gender::female, max_age);
        std::unordered_map<core::Gender, std::vector<int>> count;
        count.emplace(core::Gender::male, max_age);
        count.emplace(core::Gender::female, max_age);

        for (auto &person : context.population()) {
            if (!person.is_active()) {
                continue;
            }
            mean[factor_name][person.gender][person.age] += person.risk_factors[factor_name];
            count[person.gender][person.age] += 1;
        }

        for (const auto &[sex, count_by_age] : count) {
            for (int age = 0; age < max_age; age++) {
                if (count_by_age[age] > 0) {
                    mean[factor_name][sex][age] /= count_by_age[age];
                }
            }
        }
    }

    return mean;
}

StaticLinearModelDefinition::StaticLinearModelDefinition(
    std::vector<core::Identifier> risk_factor_names, LinearModelParams risk_factor_models,
    Eigen::MatrixXd risk_factor_cholesky)
    : risk_factor_names_{std::move(risk_factor_names)},
      risk_factor_models_{std::move(risk_factor_models)},
      risk_factor_cholesky_{std::move(risk_factor_cholesky)} {

    if (risk_factor_names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (risk_factor_models_.intercepts.empty() || risk_factor_models_.coefficients.empty()) {
        throw core::HgpsException("Risk factor models mapping is incomplete");
    }
    if (!risk_factor_cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
}

std::unique_ptr<HierarchicalLinearModel> StaticLinearModelDefinition::create_model() const {
    return std::make_unique<StaticLinearModel>(risk_factor_names_, risk_factor_models_,
                                               risk_factor_cholesky_);
}

} // namespace hgps
