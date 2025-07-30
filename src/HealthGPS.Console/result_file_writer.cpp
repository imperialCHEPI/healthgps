#include "result_file_writer.h"

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS/data_series.h"

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>
#include <unordered_set>

namespace hgps {
ResultFileWriter::ResultFileWriter(const std::filesystem::path &file_name, ExperimentInfo info)
    : info_{std::move(info)}, base_filename_{file_name} {
    stream_.open(file_name, std::ofstream::out | std::ofstream::app);
    if (stream_.fail() || !stream_.is_open()) {
        throw std::invalid_argument(fmt::format("Cannot open output file: {}", file_name.string()));
    }

    auto output_filename = file_name;
    output_filename.replace_extension("csv");
    csvstream_.open(output_filename, std::ofstream::out | std::ofstream::app);
    if (csvstream_.fail() || !csvstream_.is_open()) {
        throw std::invalid_argument(
            fmt::format("Cannot open output file: {}", output_filename.string()));
    }

    write_json_begin(output_filename);
}

ResultFileWriter::ResultFileWriter(ResultFileWriter &&other) noexcept
    : stream_{std::move(other.stream_)}, csvstream_{std::move(other.csvstream_)},
      info_{std::move(other.info_)}, base_filename_{std::move(other.base_filename_)} {}

ResultFileWriter &ResultFileWriter::operator=(ResultFileWriter &&other) noexcept {
    stream_.close();
    stream_ = std::move(other.stream_);

    csvstream_.close();
    csvstream_ = std::move(other.csvstream_);

    info_ = std::move(other.info_);
    base_filename_ = std::move(other.base_filename_);
    return *this;
}

ResultFileWriter::~ResultFileWriter() {
    if (stream_.is_open()) {
        write_json_end();
        stream_.flush();
        stream_.close();
    }

    if (csvstream_.is_open()) {
        csvstream_.flush();
        csvstream_.close();
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
    
    // Write income-based CSV files if income analysis is enabled
    if (message.content.population_by_income.has_value()) {
        write_income_based_csv_files(message);
    }
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
    csvstream_ << "source,run,time,gender_name,index_id";
    for (const auto &chan : message.content.series.channels()) {
        csvstream_ << "," << chan;
    }

    csvstream_ << '\n';
    csvstream_.flush();
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

        csvstream_ << mss.str() << '\n' << fss.str() << '\n';

        // Reset row streams
        mss.str(std::string{});
        mss.clear();

        fss.str(std::string{});
        fss.clear();
    }
}

void ResultFileWriter::write_income_based_csv_files(const hgps::ResultEventMessage &message) {
    auto available_income_categories = get_available_income_categories(message);
    
    for (const auto &income : available_income_categories) {
        auto income_filename = generate_income_filename(base_filename_.string(), income);
        std::ofstream income_csv(income_filename);
        
        if (income_csv.fail() || !income_csv.is_open()) {
            throw std::invalid_argument(
                fmt::format("Cannot open income-based output file: {}", income_filename));
        }
        
        // Write header for this income category
        income_csv << "source,run,time,gender_name,index_id";
        for (const auto &chan : message.content.series.channels()) {
            income_csv << "," << chan;
        }
        income_csv << '\n';
        
        // Write data for this income category
        write_income_csv_data(message, income, income_csv);
        
        income_csv.flush();
        income_csv.close();
    }
}

void ResultFileWriter::write_income_csv_data(const hgps::ResultEventMessage &message, 
                                            core::Income income, std::ofstream &income_csv) {
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
            // Use income-based data if available, otherwise use regular data
            try {
                mss << sep << series.at(Gender::male, income, key).at(index);
                fss << sep << series.at(Gender::female, income, key).at(index);
            } catch (const std::out_of_range &) {
                // Fallback to regular data if income-based data not available
                mss << sep << series.at(Gender::male, key).at(index);
                fss << sep << series.at(Gender::female, key).at(index);
            }
        }

        income_csv << mss.str() << '\n' << fss.str() << '\n';

        // Reset row streams
        mss.str(std::string{});
        mss.clear();

        fss.str(std::string{});
        fss.clear();
    }
}

std::string ResultFileWriter::generate_income_filename(const std::string &base_filename, core::Income income) const {
    auto income_suffix = income_category_to_string(income);
    auto dot_pos = base_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return base_filename.substr(0, dot_pos) + "_" + income_suffix + base_filename.substr(dot_pos);
    }
    return base_filename + "_" + income_suffix;
}

std::string ResultFileWriter::income_category_to_string(core::Income income) const {
    switch (income) {
        case core::Income::low: return "LowIncome";
        case core::Income::middle: return "MiddleIncome";
        case core::Income::high: return "HighIncome";
        case core::Income::lowermiddle: return "LowerMiddleIncome";
        case core::Income::uppermiddle: return "UpperMiddleIncome";
        case core::Income::unknown: return "UnknownIncome";
        default: return "UnknownIncome";
    }
}

std::vector<core::Income> ResultFileWriter::get_available_income_categories(const hgps::ResultEventMessage &message) const {
    std::vector<core::Income> categories;
    std::unordered_set<core::Income> seen;
    
    // Check which income categories have population data
    if (message.content.population_by_income.has_value()) {
        const auto &pop_by_income = message.content.population_by_income.value();
        if (pop_by_income.low > 0 && !seen.contains(core::Income::low)) {
            categories.push_back(core::Income::low);
            seen.insert(core::Income::low);
        }
        if (pop_by_income.middle > 0 && !seen.contains(core::Income::middle)) {
            categories.push_back(core::Income::middle);
            seen.insert(core::Income::middle);
        }
        if (pop_by_income.high > 0 && !seen.contains(core::Income::high)) {
            categories.push_back(core::Income::high);
            seen.insert(core::Income::high);
        }
    }
    
    return categories;
}
} // namespace hgps
