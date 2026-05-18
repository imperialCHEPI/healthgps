#pragma once

#include "static_linear_model.h"

#include <functional>
#include <optional>

namespace hgps {

struct LinearModelEvalOptions {
    /// @brief When set, age/age2/Age/Age2 predictors use this value instead of person.age.
    std::optional<double> capped_age;

    /// @brief Optional fallback when a predictor cannot be resolved (e.g. expected mean lookup).
    std::function<std::optional<double>(const core::Identifier &)> missing_predictor_fallback;
};

/// @brief Sum intercept + coefficients * predictor values; skips metadata rows.
double evaluate_linear_model(const Person &person, const LinearModelParams &model,
                             const LinearModelEvalOptions &options = {});

/// @brief Resolve one predictor for linear models (respects capped_age in options).
double get_linear_predictor_value(const Person &person, const core::Identifier &name,
                                  const LinearModelEvalOptions &options = {});

} // namespace hgps
