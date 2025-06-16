#include "result_file_writer.h"

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS/data_series.h"

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>

namespace hgps {
ResultFileWriter::ResultFileWriter(const std::filesystem::path &file_name, ExperimentInfo info)
    : info_{std::move(info)} {
    stream_.open(file_name, std::ofstream::out | std::ofstream::app);
    if (stream_.fail() || !stream_.is_open()) {
        throw std::invalid_argument(fmt::format("Cannot open output file: {}", file_name.string()));
    }

    // Create income-specific filenames for quartile-based output
    // This creates 4 additional CSV files, one for each income quartile (0=low, 1=lower-middle,
    // 2=upper-middle, 3=high)
    auto output_filename = file_name;
    auto output_filename_LowerInc = file_name;
    auto output_filename_LowerMiddleInc = file_name;
    auto output_filename_UpperMiddleInc = file_name;
    auto output_filename_UpperInc = file_name;

    // replace .json extension to .csv extension with clearer names
    output_filename.replace_extension("csv");
    output_filename_LowerInc.replace_extension("_Q1_LowIncome.csv");
    output_filename_LowerMiddleInc.replace_extension("_Q2_LowerMiddleIncome.csv");
    output_filename_UpperMiddleInc.replace_extension("_Q3_UpperMiddleIncome.csv");
    output_filename_UpperInc.replace_extension("_Q4_HighIncome.csv");

    /*fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold,
               "\n\n\n\n\n\n\n\n\n\nResultsFileWriter Constructor, output_filename_LowerInc = "
               "{}\n\n\n\n\n\n\n\n\n",
               output_filename_LowerInc.string());*/

    // open .csv streams for main file and all income quartiles
    csvstream_.open(output_filename, std::ofstream::out | std::ofstream::app);
    csvstream_LowerInc_.open(output_filename_LowerInc, std::ofstream::out | std::ofstream::app);
    csvstream_LowerMiddleInc_.open(output_filename_LowerMiddleInc,
                                   std::ofstream::out | std::ofstream::app);
    csvstream_UpperMiddleInc_.open(output_filename_UpperMiddleInc,
                                   std::ofstream::out | std::ofstream::app);
    csvstream_upperInc_.open(output_filename_UpperInc, std::ofstream::out | std::ofstream::app);

    // MAHIMA: comprehensive error checking for ALL streams, not just the main one
    // This ensures that all income-quartile-specific CSV files are properly opened
    if (csvstream_.fail() || !csvstream_.is_open() || csvstream_LowerInc_.fail() ||
        !csvstream_LowerInc_.is_open() || csvstream_LowerMiddleInc_.fail() ||
        !csvstream_LowerMiddleInc_.is_open() || csvstream_UpperMiddleInc_.fail() ||
        !csvstream_UpperMiddleInc_.is_open() || csvstream_upperInc_.fail() ||
        !csvstream_upperInc_.is_open()) {
        throw std::invalid_argument(
            fmt::format("Failed to open one or more CSV output files. Base file: {}",
                        output_filename.string()));
    }

    write_json_begin(output_filename);
}

// MAHIMA: Complete move constructor to handle ALL streams, not just main ones
// This prevents resource leaks and ensures proper transfer of income-quartile streams
ResultFileWriter::ResultFileWriter(ResultFileWriter &&other) noexcept
    : stream_{std::move(other.stream_)}, csvstream_{std::move(other.csvstream_)},
      csvstream_LowerInc_{std::move(other.csvstream_LowerInc_)},
      csvstream_LowerMiddleInc_{std::move(other.csvstream_LowerMiddleInc_)},
      csvstream_UpperMiddleInc_{std::move(other.csvstream_UpperMiddleInc_)},
      csvstream_upperInc_{std::move(other.csvstream_upperInc_)}, info_{std::move(other.info_)} {}

// MAHIMA: Complete move assignment operator to handle ALL streams
// This ensures proper resource management when moving ResultFileWriter objects
// This was preventing the results to be written for all quartiles and not just the highest
ResultFileWriter &ResultFileWriter::operator=(ResultFileWriter &&other) noexcept {
    // Close existing streams before moving
    stream_.close();
    stream_ = std::move(other.stream_);

    csvstream_.close();
    csvstream_ = std::move(other.csvstream_);

    // Handle income-quartile-specific streams
    csvstream_LowerInc_.close();
    csvstream_LowerInc_ = std::move(other.csvstream_LowerInc_);

    csvstream_LowerMiddleInc_.close();
    csvstream_LowerMiddleInc_ = std::move(other.csvstream_LowerMiddleInc_);

    csvstream_UpperMiddleInc_.close();
    csvstream_UpperMiddleInc_ = std::move(other.csvstream_UpperMiddleInc_);

    csvstream_upperInc_.close();
    csvstream_upperInc_ = std::move(other.csvstream_upperInc_);

    info_ = std::move(other.info_);
    return *this;
}

// MAHIMA: Remove duplicate stream closures that were causing potential crashes
// Original code had duplicate close() calls for all income-quartile streams
ResultFileWriter::~ResultFileWriter() {
    if (stream_.is_open()) {
        write_json_end();
        stream_.flush();
        stream_.close();
    }

    if (csvstream_.is_open()) {
        csvstream_.flush();
        csvstream_.close();

        // MAHIMA: Close income-quartile-specific streams (FIXED: removed duplicate calls)
        if (csvstream_LowerInc_.is_open()) {
            csvstream_LowerInc_.flush();
            csvstream_LowerInc_.close();
        }

        if (csvstream_LowerMiddleInc_.is_open()) {
            csvstream_LowerMiddleInc_.flush();
            csvstream_LowerMiddleInc_.close();
        }

        if (csvstream_UpperMiddleInc_.is_open()) {
            csvstream_UpperMiddleInc_.flush();
            csvstream_UpperMiddleInc_.close();
        }

        if (csvstream_upperInc_.is_open()) {
            csvstream_upperInc_.flush();
            csvstream_upperInc_.close();
        }
    }
}

void ResultFileWriter::write(const hgps::ResultEventMessage &message) {
    std::scoped_lock lock(lock_mutex_);
    if (first_row_.load()) {
        first_row_ = false;
        write_csv_header(message);
    } else {
        stream_ << ",";
    }

    stream_ << to_json_string(message);
    write_csv_channels(message);
}

void ResultFileWriter::write_json_begin(const std::filesystem::path &output) {
    using json = nlohmann::ordered_json;

    auto tp = std::chrono::system_clock::now();
    json msg = {
        {"experiment",
         {{"model", info_.model},
          {"version", info_.version},
          {"intervention", info_.intervention},
          {"job_id", info_.job_id},
          {"custom_seed", info_.seed},
          {"time_of_day", fmt::format("{0:%F %H:%M:}{1:%S} {0:%Z}", tp, tp.time_since_epoch())},
          {"output_filename", output.filename().string()}}},
        {"result", {1, 2}}};

    auto json_header = msg.dump();
    auto array_start = json_header.find_last_of('[');
    stream_ << json_header.substr(0, array_start + 1);
    stream_.flush();
}

void ResultFileWriter::write_json_end() { stream_ << "]}"; }

std::string ResultFileWriter::to_json_string(const hgps::ResultEventMessage &message) {
    using json = nlohmann::ordered_json;
    using namespace hgps::core;

    json msg = {
        {"id", message.id()},
        {"source", message.source},
        {"run", message.run_number},
        {"time", message.model_time},
        {"population",
         {
             {"size", message.content.population_size},
             {"alive", message.content.number_alive.total()},
             {"alive_male", message.content.number_alive.males},
             {"alive_female", message.content.number_alive.females},
             {"migrating", message.content.number_emigrated},
             {"dead", message.content.number_dead},
             {"recycle", message.content.number_of_recyclable()},
         }},
        {"average_age",
         {{"male", message.content.average_age.male},
          {"female", message.content.average_age.female}}},
        {"indicators",
         {
             {"YLL", message.content.indicators.years_of_life_lost},
             {"YLD", message.content.indicators.years_lived_with_disability},
             {"DALY", message.content.indicators.disability_adjusted_life_years},
         }},
    };

    for (const auto &factor : message.content.risk_ractor_average) {
        msg["risk_factors_average"][factor.first] = {{"male", factor.second.male},
                                                     {"female", factor.second.female}};
    }

    for (const auto &disease : message.content.disease_prevalence) {
        msg["disease_prevalence"][disease.first] = {{"male", disease.second.male},
                                                    {"female", disease.second.female}};
    }

    for (const auto &item : message.content.comorbidity) {
        msg["comorbidities"][std::to_string(item.first)] = {{"male", item.second.male},
                                                            {"female", item.second.female}};
    }

    msg["metrics"] = message.content.metrics;
    return msg.dump();
}

void ResultFileWriter::write_csv_header(const hgps::ResultEventMessage &message) {

    std::string StartOfHeaderString = "source,run,time,gender_name,index_id";

    // Write headers to all CSV files (main + 4 income quartiles)
    csvstream_ << StartOfHeaderString;
    csvstream_LowerInc_ << StartOfHeaderString;
    csvstream_LowerMiddleInc_ << StartOfHeaderString;
    csvstream_UpperMiddleInc_ << StartOfHeaderString;
    csvstream_upperInc_ << StartOfHeaderString;

    for (const auto &chan : message.content.series.channels()) {
        csvstream_ << "," << chan;
        csvstream_LowerInc_ << "," << chan;
        csvstream_LowerMiddleInc_ << "," << chan;
        csvstream_UpperMiddleInc_ << "," << chan;
        csvstream_upperInc_ << "," << chan;
    }

    csvstream_ << '\n';
    csvstream_LowerInc_ << '\n';
    csvstream_LowerMiddleInc_ << '\n';
    csvstream_UpperMiddleInc_ << '\n';
    csvstream_upperInc_ << '\n';

    // MAHIMA: Ensure all headers are written immediately
    csvstream_.flush();
    csvstream_LowerInc_.flush();
    csvstream_LowerMiddleInc_.flush();
    csvstream_UpperMiddleInc_.flush();
    csvstream_upperInc_.flush();
}

void ResultFileWriter::write_csv_channels(const hgps::ResultEventMessage &message) {
    using namespace hgps::core;

    const auto *sep = ",";
    const auto &series = message.content.series;
    std::stringstream mss;
    std::stringstream fss;

    for (auto index = 0u; index < series.sample_size(); index++) {
        mss << message.source << sep << message.run_number << sep << message.model_time << sep
            << "male" << sep << index;
        fss << message.source << sep << message.run_number << sep << message.model_time << sep
            << "female" << sep << index;
        for (const auto &key : series.channels()) {
            mss << sep << series.at(Gender::male, key).at(index);
            fss << sep << series.at(Gender::female, key).at(index);
        }

        // INCOME QUARTILE ROUTING: Extract message to appropriate .csv stream based on
        // IncomeCategory "All" -> main CSV, "0" -> low income, "1" -> lower-middle, "2" ->
        // upper-middle, "3" -> high income
        if (message.content.IncomeCategory == "All") {
            csvstream_ << mss.str() << '\n' << fss.str() << '\n';
        } else if (message.content.IncomeCategory == "0") {
            csvstream_LowerInc_ << mss.str() << '\n' << fss.str() << '\n';
        } else if (message.content.IncomeCategory == "1") {
            csvstream_LowerMiddleInc_ << mss.str() << '\n' << fss.str() << '\n';
        } else if (message.content.IncomeCategory == "2") {
            csvstream_UpperMiddleInc_ << mss.str() << '\n' << fss.str() << '\n';
        } else if (message.content.IncomeCategory == "3") {
            csvstream_upperInc_ << mss.str() << '\n' << fss.str() << '\n';
        }

        // Reset row streams for next iteration
        mss.str(std::string{});
        mss.clear();

        fss.str(std::string{});
        fss.clear();
    }
}
} // namespace hgps
