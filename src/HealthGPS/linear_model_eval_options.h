#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include <functional>
#include <optional>

namespace hgps {

struct LinearModelEvalOptions {
    /// @brief When set, age/age2/Age/Age2 predictors use this value instead of person.age.
    std::optional<double> capped_age;

    /// @brief When set, gender2 returns 1 for this sex and 0 for the other (reference category).
    std::optional<core::Gender> gender2_indicator;

    /// @brief Optional fallback when a predictor cannot be resolved (e.g. expected mean lookup).
    std::function<std::optional<double>(const core::Identifier &)> missing_predictor_fallback;
};

} // namespace hgps
