#include "food_labelling_scenario.h"

namespace hgps {
/// @brief Food labelling: the entity qualify but is not affected by the policy identifier
inline constexpr int FOP_NO_EFFECT = -2;

FoodLabellingScenario::FoodLabellingScenario(SyncChannel &data_sync, FoodLabellingDefinition &&definition)
    : channel_{data_sync}, definition_{std::move(definition)} 
{
    if (definition_.impacts.empty()) 
        throw std::invalid_argument("Number of impact levels mismatch, must be greater than 1.");

    auto age = 0u;
    for (const auto &level : definition_.impacts) 
    {
        if (level.from_age < age) 
            throw std::out_of_range("Impact levels must be non-overlapping and ordered.");

        if (!factor_impact_.contains(level.risk_factor)) 
            factor_impact_.emplace(level.risk_factor);

        age = level.from_age + 1u;
    }
}

SyncChannel &FoodLabellingScenario::channel() { return channel_; }

void FoodLabellingScenario::clear() noexcept { interventions_book_.clear(); }

double FoodLabellingScenario::apply(Random &generator, Person &entity, int time, const core::Identifier &risk_factor_key, double value) 
{
    if (!definition_.active_period.contains(time) || !factor_impact_.contains(risk_factor_key))  return value;

    if (entity.age < definition_.impacts.at(0).from_age) return value;

    auto impact = value;
    auto probability = generator.next_double();
    auto time_since_start = static_cast<unsigned int>(time - definition_.active_period.start_time);
    if (time_since_start < definition_.coverage.cutoff_time) 
    {
        if (!interventions_book_.contains(entity.id()) || interventions_book_.at(entity.id()) == FOP_NO_EFFECT) 
        {
            if (probability < definition_.coverage.short_term_rate) 
            {
                impact += calculate_policy_impact(entity);
                interventions_book_.try_emplace(entity.id(), time);
            } 
            else interventions_book_.try_emplace(entity.id(), FOP_NO_EFFECT);
        }
    } 
    else if (!interventions_book_.contains(entity.id())) 
    {
        if (probability < definition_.coverage.long_term_rate) 
        {
            impact += calculate_policy_impact(entity);
            interventions_book_.emplace(entity.id(), time);
        } 
        else interventions_book_.emplace(entity.id(), FOP_NO_EFFECT);
    }
    return impact;
}

const PolicyInterval &FoodLabellingScenario::active_period() const noexcept {   return definition_.active_period;   }

const std::vector<PolicyImpact> &FoodLabellingScenario::impacts() const noexcept {  return definition_.impacts;     }

double FoodLabellingScenario::calculate_policy_impact(const Person &entity) const noexcept 
{
    auto interventionEffect     = this->definition_.impacts.at(0).value;
    auto currentRiskFactorValue = entity.get_risk_factor_value(definition_.adjustment_risk_factor.identifier);
    auto transferCoefficient    = definition_.transfer_coefficient.get_value(entity.gender, entity.age);
    double adjustmentFactor     = definition_.adjustment_risk_factor.adjustment;

    return transferCoefficient * interventionEffect * currentRiskFactorValue * adjustmentFactor;
}
} // namespace hgps
