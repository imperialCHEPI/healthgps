#pragma once

#include "HealthGPS.Core/identifier.h"
#include "person.h"

#include <optional>
#include <string>

namespace hgps {

/// @brief True for CSV/JSON rows that are model metadata, not regression predictors.
bool is_metadata_predictor(const core::Identifier &name);
bool is_metadata_predictor(const std::string &name);

/// @brief Resolve a predictor name from person attributes (age polynomials, dummies, log_*, income_*).
/// @return Value if the name is a known derived predictor; nullopt if not handled here.
std::optional<double> resolve_derived_predictor(const Person &person, const std::string &key);

} // namespace hgps
