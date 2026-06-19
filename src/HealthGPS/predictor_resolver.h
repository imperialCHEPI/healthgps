#pragma once

#include "HealthGPS.Core/identifier.h"
#include "person.h"

#include <optional>
#include <string>

namespace hgps {

/// @brief True for CSV/JSON rows that are model metadata, not regression predictors.
bool is_metadata_predictor(const core::Identifier &name);
bool is_metadata_predictor(const std::string &name);

/// @brief Resolve a predictor name from person attributes (age polynomials, dummies, log_*,
/// income_*).
/// @return Value if the name is a known derived predictor; nullopt if not handled here.
std::optional<double> resolve_derived_predictor(const Person &person, const std::string &key);

/// @brief True when the predictor name is the gender2 regression dummy row.
bool is_gender2_predictor(const std::string &key);

/// @brief gender2 regression value: 1 if person matches indicator_sex, else 0.
double gender2_regression_value(const Person &person, core::Gender indicator_sex);

/// @brief Parse project_requirements.demographics.gender2 ("male" or "female").
core::Gender parse_gender2_indicator(const std::string &indicator_label);

} // namespace hgps
