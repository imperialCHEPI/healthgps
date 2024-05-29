#include "pch.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS/nutrients.h"
#include "HealthGPS/program_path.h"

#include <filesystem>
#include <sstream>

using namespace hgps;

namespace {
const std::vector<core::Identifier> nutrients = {"A"_id, "B"_id, "C"_id};
} // anonymous namespace

TEST(LoadNutrientTable, GetNutrientIndexes) {
    {
        // Note that the case doesn't match; should be case insensitive
        const std::vector<std::string> column_names = {"d", "e", "a", "c", "b"};
        const auto idx = detail::get_nutrient_indexes(column_names, nutrients);
        const std::vector<size_t> expected{2, 4, 3};
        EXPECT_EQ(idx, expected);
    }

    // Missing nutrient
    {
        // "B" is missing
        const std::vector<std::string> column_names = {"D", "E", "A", "C"};
        EXPECT_THROW(detail::get_nutrient_indexes(column_names, nutrients), core::HgpsException);
    }
}

TEST(LoadNutrientTable, LoadNutrientTable) {
    const auto csv_path = (hgps::get_program_directory() / "nutrient_table.csv").string();
    const auto table = load_nutrient_table(csv_path, nutrients);

    const decltype(table) expected = {
        {"chicken", {3, 5, 4}}, {"crisps", {8, 10, 9}}, {"tofu", {13, 15, 14}}};
    EXPECT_EQ(table, expected);
}
