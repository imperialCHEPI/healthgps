#include "dummy_model.h"

namespace hgps {

DummyModel::DummyModel(RiskFactorModelType type, const std::vector<core::Identifier> &names,
                       const std::vector<double> &values, const std::vector<double> &policy)
    : type_{type}, names_{names}, values_{values}, policy_{policy} {
    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (values_.empty()) {
        throw core::HgpsException("Risk factor values list is empty");
    }
    if (policy_.empty()) {
        throw core::HgpsException("Risk factor policy lisy empty");
    }
}

RiskFactorModelType DummyModel::type() const noexcept { return type_; }

std::string DummyModel::name() const noexcept { return "Dummy"; }

void DummyModel::generate_risk_factors(RuntimeContext &context) {

    // Initialise everyone.
    for (auto &entity : context.population()) {
        set_risk_factors(entity, context.scenario().type());
    }
}

void DummyModel::update_risk_factors(RuntimeContext &context) {

    // Initialise everyone except inactive.
    for (auto &entity : context.population()) {
        // Ignore if inactive.
        if (!entity.is_active()) {
            continue;
        }

        set_risk_factors(entity, context.scenario().type());
    }
}

void DummyModel::set_risk_factors(Person &person, ScenarioType scenario) const {
    for (auto i = 0u; i < names_.size(); ++i) {
        person.risk_factors[names_[i]] = values_[i];

        // Apply intervention policy.
        if (scenario == ScenarioType::intervention) {
            person.risk_factors[names_[i]] += policy_[i];
        }
    }
}

DummyModelDefinition::DummyModelDefinition(RiskFactorModelType type,
                                           std::vector<core::Identifier> names,
                                           std::vector<double> values, std::vector<double> policy)
    : type_{type}, names_{std::move(names)}, values_{std::move(values)},
      policy_{std::move(policy)} {
    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (values_.empty()) {
        throw core::HgpsException("Risk factor values list is empty");
    }
    if (policy_.empty()) {
        throw core::HgpsException("Risk factor policy lisy empty");
    }
}

std::unique_ptr<RiskFactorModel> DummyModelDefinition::create_model() const {
    return std::make_unique<DummyModel>(type_, names_, values_, policy_);
}

} // namespace hgps
