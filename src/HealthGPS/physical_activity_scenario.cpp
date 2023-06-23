#include "physical_activity_scenario.h"

namespace hgps {
/// @brief Physical activity: children effect identifier
inline constexpr int PA_CHILD_EFFECT = -1;

/// @brief Physical activity: no-effect identifier
inline constexpr int PA_NO_EFFECT = -2;

PhysicalActivityScenario::PhysicalActivityScenario(SyncChannel &data_sync,
                                                   PhysicalActivityDefinition &&definition)
    : channel_{data_sync}, definition_{std::move(definition)}, factor_impact_{},
      interventions_book_{} {
    if (definition_.impacts.size() < 2) {
        throw std::invalid_argument("Number of impact levels mismatch, must be greater than 2.");
    }

    auto age = 0u;
    for (const auto &level : definition_.impacts) {
        if (level.from_age < age) {
            throw std::invalid_argument("Impact levels must be non-overlapping and ordered.");
        }

        if (!factor_impact_.contains(level.risk_factor)) {
            factor_impact_.emplace(level.risk_factor);
        }

        age = level.from_age + 1u;
    }
}

SyncChannel &PhysicalActivityScenario::channel() { return channel_; }

void PhysicalActivityScenario::clear() noexcept { interventions_book_.clear(); }

double PhysicalActivityScenario::apply(Random &generator, Person &entity, int time,
                                       const core::Identifier &risk_factor_key, double value) {
    if (!definition_.active_period.contains(time) || !factor_impact_.contains(risk_factor_key)) {
        return value;
    }

    auto &child_effect = definition_.impacts.at(0);
    if (entity.age < child_effect.from_age) {
        return value;
    }

    auto impact = value;
    auto probability = generator.next_double();
    if (!interventions_book_.contains(entity.id())) {
        if (entity.age > child_effect.to_age) {
            interventions_book_.emplace(entity.id(), PA_NO_EFFECT);
        } else if (probability < definition_.coverage_rate) {
            impact += child_effect.value;
            interventions_book_.emplace(entity.id(), PA_CHILD_EFFECT);
        } else {
            interventions_book_.emplace(entity.id(), PA_NO_EFFECT);
        }
    } else if (interventions_book_.at(entity.id()) == PA_CHILD_EFFECT) {
        if (entity.age > child_effect.to_age) {
            auto &adult_risk = definition_.impacts.at(1);
            impact += adult_risk.value - child_effect.value;
            interventions_book_.at(entity.id()) = time;
        }
    }

    return impact;
}

const PolicyInterval &PhysicalActivityScenario::active_period() const noexcept {
    return definition_.active_period;
}

const std::vector<PolicyImpact> &PhysicalActivityScenario::impacts() const noexcept {
    return definition_.impacts;
}
} // namespace hgps