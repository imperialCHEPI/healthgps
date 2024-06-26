#include "marketing_dynamic_scenario.h"
namespace hgps {

/// @brief Dynamic marketing: Never applied the intervention status identifier
inline constexpr int MARKETING_NEVER = -1;

/// @brief Dynamic marketing: Previous applied the intervention status identifier
inline constexpr int MARKETING_FORMER = -2;

hgps::MarketingDynamicScenario::MarketingDynamicScenario(SyncChannel &data_sync,
                                                         MarketingDynamicDefinition &&definition)
    : channel_{data_sync}, definition_{std::move(definition)} {
    if (definition_.impacts.empty()) {
        throw std::invalid_argument("Number of impact levels mismatch, must be greater than 1.");
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

SyncChannel &MarketingDynamicScenario::channel() { return channel_; }

void MarketingDynamicScenario::clear() noexcept { interventions_book_.clear(); }

double MarketingDynamicScenario::apply(Random &generator, Person &entity, int time,
                                       const core::Identifier &risk_factor_key, double value) {
    if (!definition_.active_period.contains(time) || !factor_impact_.contains(risk_factor_key)) {
        return value;
    }

    if (entity.age < definition_.impacts.at(0).from_age) {
        return value;
    }

    auto current_impact_index = get_index_of_impact_by_age(entity.age);
    if (current_impact_index < 0) {
        return value;
    }

    auto impact = value;
    auto probability = generator.next_double();
    if (!interventions_book_.contains(entity.id())) {
        // Never intervened
        if (probability < definition_.dynamic.alpha) {
            impact += get_differential_impact(current_impact_index, MARKETING_NEVER);
            interventions_book_.emplace(entity.id(), current_impact_index);
        }
    } else {
        auto &current_policy_status = interventions_book_.at(entity.id());
        if (current_policy_status == MARKETING_FORMER) {
            if (probability < definition_.dynamic.gamma) {
                impact += get_differential_impact(current_impact_index, MARKETING_FORMER);
                current_policy_status = current_impact_index;
            }
        } else if (probability < definition_.dynamic.beta) {
            impact += get_differential_impact(MARKETING_FORMER, current_policy_status);
            current_policy_status = MARKETING_FORMER;
        } else { // 1 - beta
            impact += get_differential_impact(current_impact_index, current_policy_status);
            current_policy_status = current_impact_index;
        }
    }

    return impact;
}

const PolicyInterval &MarketingDynamicScenario::active_period() const noexcept {
    return definition_.active_period;
}

const std::vector<PolicyImpact> &MarketingDynamicScenario::impacts() const noexcept {
    return definition_.impacts;
}

const PolicyDynamic &MarketingDynamicScenario::dynamic() const noexcept {
    return definition_.dynamic;
}

int MarketingDynamicScenario::get_index_of_impact_by_age(unsigned int age) const {
    for (auto index = 0; const auto &policy : definition_.impacts) {
        if (policy.contains(age)) {
            return index;
        }

        index++;
    }

    return MARKETING_NEVER;
}

double MarketingDynamicScenario::get_differential_impact(int current_index,
                                                         int previous_index) const {
    auto current_impact = 0.0;
    if (current_index > MARKETING_NEVER) {
        current_impact = definition_.impacts.at(current_index).value;
    }

    auto previous_impact = 0.0;
    if (previous_index > MARKETING_NEVER) {
        previous_impact = definition_.impacts.at(previous_index).value;
    }

    return current_impact - previous_impact;
}
} // namespace hgps
