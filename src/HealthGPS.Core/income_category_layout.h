#pragma once

#include "forward_type.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace hgps::core {

/// @brief Ordered final income buckets for project_requirements.income.categories (3, 4, or 5).
struct IncomeCategoryLayout {
    std::size_t count{};
    std::vector<Income> strata;
    std::vector<std::string> labels;
};

/// @brief Parse project_requirements.income.categories ("3", "4", or "5").
/// @throws HgpsException if the value is not supported.
IncomeCategoryLayout income_category_layout_from_config(std::string_view categories);

/// @brief Map person.income to a 0-based table index for the configured layout.
/// @throws HgpsException if income is not in layout.strata.
std::size_t income_table_index(Income income, const IncomeCategoryLayout &layout);

/// @brief Map equal-split rank bucket (0..count-1) to Income enum.
Income income_from_equal_split_bucket(std::size_t bucket, const IncomeCategoryLayout &layout);

/// @brief Project-aware numeric encoding for analysis/output (legacy 3/4 mapping preserved).
double income_category_numeric(Income income, const IncomeCategoryLayout &layout);

} // namespace hgps::core
