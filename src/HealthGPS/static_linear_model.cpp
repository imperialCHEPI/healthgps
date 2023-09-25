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

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {}

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
