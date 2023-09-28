#pragma once

#include "HealthGPS.Core/identifier.h"

#include "rapidcsv.h"

#include <unordered_map>
#include <vector>

namespace hgps {
namespace detail {
/// @brief Work out which column each macronutrient is in
/// @param column_names Column names for CSV file
/// @param nutrients Macronutrients
/// @return Index for each macronutrient in order
std::vector<size_t> get_nutrient_indexes(const std::vector<std::string> &column_names,
                                         const std::vector<core::Identifier> &nutrients);

std::unordered_map<core::Identifier, std::vector<double>>
get_nutrient_table(rapidcsv::Document &doc, const std::vector<core::Identifier> &nutrients,
                   const std::vector<size_t> &nutrient_idx);
} // namespace detail

/// @brief Load nutrient information from a CSV file
/// @param csv_path Path to CSV file
/// @param nutrients Names of macronutrients
/// @return Mapping from food name to constituent macronutrients
std::unordered_map<core::Identifier, std::vector<double>>
load_nutrient_table(const std::string &csv_path, const std::vector<core::Identifier> &nutrients);
} // namespace hgps
