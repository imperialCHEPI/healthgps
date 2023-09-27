#include "pch.h"
#include "program_path.h"

#include "HealthGPS.Core/exception.h"
#include "HealthGPS/nutrients.h"

#include <filesystem>
#include <sstream>

using namespace hgps;

namespace {
constexpr auto *csv_data = R"(food,d,e,a,c,b
chicken,1,2,3,4,5
crisps,6,7,8,9,10
tofu,11,12,13,14,15
)";

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

TEST(LoadNutrientTable, GetNutrientTable) {
    std::istringstream is{csv_data};
    rapidcsv::Document doc{is, rapidcsv::LabelParams{0, 0}};
    const auto table = detail::get_nutrient_table(doc, nutrients, {2, 4, 3});

    const decltype(table) expected = {
        {"chicken", {3, 5, 4}}, {"crisps", {8, 10, 9}}, {"tofu", {13, 15, 14}}};
    EXPECT_EQ(table, expected);
}

TEST(LoadNutrientTable, LoadNutrientTable) {
    const auto csv_path = (get_program_directory() / "nutrient_table.csv").string();
    const auto table = load_nutrient_table(csv_path, nutrients);

    const decltype(table) expected = {
        {"chicken", {3, 5, 4}}, {"crisps", {8, 10, 9}}, {"tofu", {13, 15, 14}}};
    EXPECT_EQ(table, expected);
}
