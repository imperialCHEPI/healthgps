#include "pch.h"

#include "HealthGPS.Console/model_info.h"
#include "HealthGPS.Console/result_file_writer.h"
#include "HealthGPS/result_message.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
std::vector<std::string> read_lines(const std::filesystem::path &path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}
} // namespace

TEST(ResultFileWriter, IncomeCsvIncludesZeroCountAgeGenderRows) {
    using namespace hgps;
    using namespace hgps::core;

    const auto temp_dir =
        std::filesystem::temp_directory_path() /
        ("healthgps_income_writer_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp_dir);
    const auto base_file = temp_dir / "result.json";

    auto result = ModelResult(3u);
    result.series.add_channels({"count", "mean_income_category"});
    result.series.add_income_channels_for_categories({"count", "mean_income_category"},
                                                     {Income::low});

    // MAHIMA: Keep middle index at zero counts to assert we still write rows for empty strata.
    result.series.at(Gender::male, Income::low, "count").at(0) = 1.0;
    result.series.at(Gender::female, Income::low, "count").at(0) = 0.0;
    result.series.at(Gender::male, Income::low, "count").at(1) = 0.0;
    result.series.at(Gender::female, Income::low, "count").at(1) = 0.0;
    result.series.at(Gender::male, Income::low, "count").at(2) = 2.0;
    result.series.at(Gender::female, Income::low, "count").at(2) = 3.0;

    result.population_by_income = ResultByIncome{};
    result.population_by_income->low = 1.0;

    const auto message = ResultEventMessage("baseline", 1u, 2022, std::move(result));
    auto info = ExperimentInfo{
        .model = "test", .version = "1", .intervention = "none", .job_id = 1, .seed = 1u};

    {
        auto writer = ResultFileWriter(base_file, info, true);
        writer.write(message);
    }

    const auto income_file = temp_dir / "result_LowIncome.csv";
    const auto lines = read_lines(income_file);
    ASSERT_FALSE(lines.empty());

    // Header + 3 ages * 2 genders = 7 lines expected.
    EXPECT_EQ(7u, lines.size());

    // Ensure index 1 (zero counts for both genders) still has two rows.
    std::size_t index1_rows = 0u;
    for (const auto &line : lines) {
        const auto first_comma = line.find(',');
        if (first_comma == std::string::npos) {
            continue;
        }
        const auto second_comma = line.find(',', first_comma + 1);
        const auto third_comma = line.find(',', second_comma + 1);
        const auto fourth_comma = line.find(',', third_comma + 1);
        const auto fifth_comma = line.find(',', fourth_comma + 1);
        if (fifth_comma == std::string::npos) {
            continue;
        }

        const auto gender = line.substr(third_comma + 1, fourth_comma - (third_comma + 1));
        const auto index = line.substr(fourth_comma + 1, fifth_comma - (fourth_comma + 1));
        if (index == "1" && (gender == "male" || gender == "female")) {
            ++index1_rows;
        }
    }
    EXPECT_EQ(2u, index1_rows);

    std::filesystem::remove_all(temp_dir);
}
