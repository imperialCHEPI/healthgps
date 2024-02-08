#include "dummy_model.h"

namespace hgps {

DummyModel::DummyModel(RiskFactorModelType type, const std::vector<core::Identifier> &names,
                       const std::vector<double> &values, const std::vector<double> &policy,
                       const std::vector<int> &policy_start)
    : type_{type}, names_{names}, values_{values}, policy_{policy}, policy_start_{policy_start} {}

RiskFactorModelType DummyModel::type() const noexcept { return type_; }

std::string DummyModel::name() const noexcept { return "Dummy"; }

void DummyModel::generate_risk_factors(RuntimeContext &context) {

    // Initialise everyone.
    for (auto &entity : context.population()) {
        set_risk_factors(entity, context.scenario().type(), 0);
    }
}

void DummyModel::update_risk_factors(RuntimeContext &context) {

    // Initialise everyone except inactive.
    for (auto &entity : context.population()) {
        // Ignore if inactive.
        if (!entity.is_active()) {
            continue;
        }

        int time_elapsed = context.time_now() - context.start_time();
        set_risk_factors(entity, context.scenario().type(), time_elapsed);
    }
}

void DummyModel::set_risk_factors(Person &person, ScenarioType scenario, int time_elapsed) const {
    for (auto i = 0u; i < names_.size(); i++) {
        person.risk_factors[names_[i]] = values_[i];

        // Apply policy to factor if intervening and start time is reached.
        if (scenario == ScenarioType::intervention && time_elapsed >= policy_start_[i]) {
            person.risk_factors[names_[i]] += policy_[i];
        }
    }
}

DummyModelDefinition::DummyModelDefinition(RiskFactorModelType type,
                                           std::vector<core::Identifier> names,
                                           std::vector<double> values, std::vector<double> policy,
                                           std::vector<int> policy_start)
    : type_{type}, names_{std::move(names)}, values_{std::move(values)}, policy_{std::move(policy)},
      policy_start_{std::move(policy_start)} {

    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (values_.empty()) {
        throw core::HgpsException("Risk factor values list is empty");
    }
    if (policy_.empty()) {
        throw core::HgpsException("Risk factor policy lisy empty");
    }
    if (policy_start_.empty()) {
        throw core::HgpsException("Risk factor policy start list is empty");
    }
    if (names_.size() != values_.size() || names_.size() != policy_.size() ||
        names_.size() != policy_start_.size()) {
        throw core::HgpsException("Risk factor param list sizes mismatch");
    }
}

std::unique_ptr<RiskFactorModel> DummyModelDefinition::create_model() const {
    return std::make_unique<DummyModel>(type_, names_, values_, policy_, policy_start_);
}

} // namespace hgps
