#include "marketing_scenario.h"

namespace hgps {
    MarketingPolicyScenario::MarketingPolicyScenario(
        SyncChannel& data_sync, MarketingPolicyDefinition&& definition)
        : channel_{ data_sync }, definition_{ definition },
        factor_impact_{}, interventions_book_{} {

        if (definition_.impacts.size() != 3) {
            throw std::invalid_argument("Number of impact levels mismatch, must be 3.");
        }

        auto age = 0u;
        for (auto& level : definition_.impacts) {
            auto factor_key = core::to_lower(level.risk_factor);
            if (level.from_age < age) {
                throw std::invalid_argument(
                    "Impact levels must be non-overlapping and ordered.");
            }

            if (!factor_impact_.contains(factor_key)) {
                factor_impact_.emplace(factor_key);
            }

            age = level.from_age + 1u;
        }
    }

    ScenarioType MarketingPolicyScenario::type() const noexcept {
        return ScenarioType::intervention;
    }

    std::string MarketingPolicyScenario::name() const noexcept {
        return "Intervention";
    }
    SyncChannel& MarketingPolicyScenario::channel() {
        return channel_;
    }
    void MarketingPolicyScenario::clear() noexcept {
        interventions_book_.clear();
    }

    double MarketingPolicyScenario::apply(Person& entity, const int& time,
        const std::string& risk_factor_key, const double& value) {
        if (!factor_impact_.contains(risk_factor_key) ||
            !definition_.active_period.contains(time)) {
            return value;
        }

        auto age = entity.age;
        auto child_effect = definition_.impacts.at(0);
        if (age < child_effect.from_age) {
            return value;
        }

        // children
        if (child_effect.contains(age)) {
            if (!interventions_book_.contains(entity.id())) {
                interventions_book_.emplace(entity.id(), age);
                return value + child_effect.value;
            }

            return value;
        }

        // adolescents
        auto teen_effect = definition_.impacts.at(1);
        if (teen_effect.contains(age)) {
            // never exposed to the intervention
            if (!interventions_book_.contains(entity.id())) {
                interventions_book_.emplace(entity.id(), age);
                return value + teen_effect.value;
            }

            // exposed as a child - IF not needed
            if (child_effect.contains(interventions_book_.at(entity.id()))) {
                interventions_book_.at(entity.id()) = age;
                auto effect = teen_effect.value - child_effect.value;
                return value + effect;
            }
        }

        auto adult_effect = definition_.impacts.at(2);
        if (age >= adult_effect.from_age) {
            // never exposed to the intervention
            if (!interventions_book_.contains(entity.id())) {
                interventions_book_.emplace(entity.id(), age);
                return value + adult_effect.value;
            }

            // adolescent to adult transition - IF not needed
            if (teen_effect.contains(interventions_book_.at(entity.id()))) {
                interventions_book_.at(entity.id()) = age;
                auto effect = adult_effect.value - teen_effect.value;
                return value + effect;
            }
        }

        return value;
    }

    const PolicyInterval& MarketingPolicyScenario::active_period() const noexcept {
        return definition_.active_period;
    }

    const std::vector<PolicyImpact>& MarketingPolicyScenario::impacts() const noexcept {
        return definition_.impacts;
    }
}
