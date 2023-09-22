#include "static_linear_model.h"
#include "runtime_context.h"

#include "HealthGPS.Core/exception.h"

namespace hgps {

StaticLinearModel::StaticLinearModel(
    const std::unordered_map<core::Identifier, LinearModel> &models,
    const std::map<int, HierarchicalLevel> &levels)
    : models_{models}, levels_{levels} {

    if (models_.empty()) {
        throw core::HgpsException("The hierarchical model equations definition must not be empty");
    }

    if (levels_.empty()) {
        throw core::HgpsException("The hierarchical model levels definition must not be empty");
    }
}

HierarchicalModelType StaticLinearModel::type() const noexcept {
    return HierarchicalModelType::Static;
}

std::string StaticLinearModel::name() const noexcept { return "Static"; }

void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {
    std::vector<MappingEntry> level_factors;
    std::unordered_map<int, std::vector<MappingEntry>> level_factors_cache;
    for (auto &entity : context.population()) {
        for (auto level = 1; level <= context.mapping().max_level(); level++) {
            if (level_factors_cache.contains(level)) {
                level_factors = level_factors_cache.at(level);
            } else {
                level_factors = context.mapping().at_level(level);
                level_factors_cache.emplace(level, level_factors);
            }

            generate_for_entity(context, entity, level, level_factors);
        }
    }
}

void StaticLinearModel::update_risk_factors(RuntimeContext &context) {
    std::vector<MappingEntry> level_factors;
    std::unordered_map<int, std::vector<MappingEntry>> level_factors_cache;
    auto newborn_age = 0u;
    for (auto &entity : context.population()) {
        if (entity.age > newborn_age) {
            continue;
        }

        for (auto level = 1; level <= context.mapping().max_level(); level++) {
            if (level_factors_cache.contains(level)) {
                level_factors = level_factors_cache.at(level);
            } else {
                level_factors = context.mapping().at_level(level);
                level_factors_cache.emplace(level, level_factors);
            }

            generate_for_entity(context, entity, level, level_factors);
        }
    }
}

void StaticLinearModel::generate_for_entity(RuntimeContext &context, Person &entity, int level,
                                            std::vector<MappingEntry> &level_factors) {
    const auto &level_info = levels_.at(level);

    // Residual Risk Factors Random Sampling
    auto residual_risk_factors = std::map<core::Identifier, double>();
    for (const auto &factor : level_factors) {
        auto row_idx = context.random().next_int(
            static_cast<int>(level_info.residual_distribution.rows() - 1));
        auto col_idx = level_info.variables.at(factor.key());
        residual_risk_factors.emplace(factor.key(),
                                      level_info.residual_distribution(row_idx, col_idx));
    }

    // The Stochastic Component of The Risk Factors
    auto stoch_comp_factors = std::map<core::Identifier, double>();
    for (const auto &factor_row : level_factors) {
        auto row_sum = 0.0;
        auto row_idx = level_info.variables.at(factor_row.key());
        for (const auto &factor_col : level_factors) {
            auto col_idx = level_info.variables.at(factor_col.key());
            row_sum +=
                level_info.transition(row_idx, col_idx) * residual_risk_factors[factor_col.key()];
        }

        stoch_comp_factors.emplace(factor_row.key(), row_sum);
    }

    // The Deterministic Risk Factors
    auto determ_risk_factors = std::map<core::Identifier, double>();
    determ_risk_factors.emplace(InterceptKey, entity.get_risk_factor_value(InterceptKey));
    for (const auto &item : context.mapping()) {
        if (item.level() < level) {
            determ_risk_factors.emplace(item.key(), entity.get_risk_factor_value(item.key()));
        }
    }

    // The Deterministic Components of Risk Factors
    auto determ_comp_factors = std::map<core::Identifier, double>();
    for (const auto &factor : level_factors) {
        auto sum = 0.0;
        for (const auto &coeff : models_.at(factor.key()).coefficients) {
            sum += coeff.second.value * determ_risk_factors[coeff.first];
        }

        determ_comp_factors.emplace(factor.key(), sum);
    }

    for (const auto &factor : level_factors) {
        auto total_value =
            determ_comp_factors.at(factor.key()) + stoch_comp_factors.at(factor.key());
        entity.risk_factors[factor.key()] = factor.get_bounded_value(total_value);
    }
}

HierarchicalLinearModelDefinition::HierarchicalLinearModelDefinition(
    std::unordered_map<core::Identifier, LinearModel> linear_models,
    std::map<int, HierarchicalLevel> model_levels)
    : models_{std::move(linear_models)}, levels_{std::move(model_levels)} {

    if (models_.empty()) {
        throw core::HgpsException("The hierarchical model equations definition must not be empty");
    }

    if (levels_.empty()) {
        throw core::HgpsException("The hierarchical model levels definition must not be empty");
    }
}

std::unique_ptr<HierarchicalLinearModel> HierarchicalLinearModelDefinition::create_model() const {
    return std::make_unique<StaticLinearModel>(models_, levels_);
}

} // namespace hgps
