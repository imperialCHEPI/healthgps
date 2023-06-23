#include "fiscal_scenario.h"
#include "HealthGPS.Core/string_util.h"

#include <fmt/core.h>
#include <stdexcept>

namespace hgps {
FiscalPolicyScenario::FiscalPolicyScenario(SyncChannel &data_sync,
                                           FiscalPolicyDefinition &&definition)
    : channel_{data_sync}, definition_{std::move(definition)}, factor_impact_{},
      interventions_book_{} {

    if (definition_.impacts.size() != 3) {
        throw std::invalid_argument("Number of impact levels mismatch, must be 3.");
    }

    auto age = 0u;
    for (const auto &level : definition_.impacts) {
        if (level.from_age < age) {
            throw std::out_of_range("Impact levels must be non-overlapping and ordered.");
        }

        if (!factor_impact_.contains(level.risk_factor)) {
            factor_impact_.emplace(level.risk_factor);
        }

        age = level.from_age + 1u;
    }
}

SyncChannel &FiscalPolicyScenario::channel() { return channel_; }

void FiscalPolicyScenario::clear() noexcept { interventions_book_.clear(); }

double FiscalPolicyScenario::apply([[maybe_unused]] Random &generator, Person &entity, int time,
                                   const core::Identifier &risk_factor_key, double value) {
    if (!factor_impact_.contains(risk_factor_key) || !definition_.active_period.contains(time)) {
        return value;
    }

    auto age = entity.age;
    auto &child_effect = definition_.impacts.at(0);
    if (age < child_effect.from_age) {
        return value;
    }

    // children
    auto factor_value = entity.get_risk_factor_value(risk_factor_key);
    if (child_effect.contains(age)) {
        if (!interventions_book_.contains(entity.id())) {
            interventions_book_.emplace(entity.id(), age);
            return value + (factor_value * child_effect.value);
        }

        return value;
    }

    // adolescents
    auto &teen_effect = definition_.impacts.at(1);
    if (teen_effect.contains(age)) {
        // never exposed to the intervention
        if (!interventions_book_.contains(entity.id())) {
            interventions_book_.emplace(entity.id(), age);
            return value + (factor_value * teen_effect.value);
        }

        // exposed as a child - IF not needed - assert!
        if (child_effect.contains(interventions_book_.at(entity.id()))) {
            interventions_book_.at(entity.id()) = age;
            auto effect = teen_effect.value - child_effect.value;
            return value + (factor_value * effect);
        }

        return value;
    }

    auto &adult_effect = definition_.impacts.at(2);
    if (age >= adult_effect.from_age) {
        // never exposed to the intervention
        if (!interventions_book_.contains(entity.id())) {
            interventions_book_.emplace(entity.id(), age);
            return value + (factor_value * adult_effect.value);
        }

        // adolescent to adult transition - IF not needed
        if (teen_effect.contains(interventions_book_.at(entity.id()))) {
            if (definition_.impact_type == FiscalImpactType::pessimist) {
                interventions_book_.at(entity.id()) = age;
                auto effect = adult_effect.value - teen_effect.value;
                return value + (factor_value * effect);
            } else if (definition_.impact_type == FiscalImpactType::optimist) {
                return value;
            } else {
                throw std::logic_error("Fiscal intervention impact type not implemented.");
            }
        }

        return value;
    }

    throw std::logic_error(
        fmt::format("Fiscal intervention error, age {} outside the valid range.", age));
}

const PolicyInterval &FiscalPolicyScenario::active_period() const noexcept {
    return definition_.active_period;
}

const std::vector<PolicyImpact> &FiscalPolicyScenario::impacts() const noexcept {
    return definition_.impacts;
}

FiscalImpactType parse_fiscal_impact_type(std::string impact_type) {
    if (core::case_insensitive::equals(impact_type, "pessimist")) {
        return FiscalImpactType::pessimist;
    } else if (core::case_insensitive::equals(impact_type, "optimist")) {
        return FiscalImpactType::optimist;
    }

    throw std::logic_error("Unknown fiscal policy impact type: " + impact_type);
}
} // namespace hgps