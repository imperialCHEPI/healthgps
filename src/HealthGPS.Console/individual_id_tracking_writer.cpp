// MAHIMA: Individual ID tracking CSV writer – allows users to verify same person (by id)
// across baseline and intervention in the _IndividualIDTracking.csv file.
#include "individual_id_tracking_writer.h"

#include <algorithm>
#include <fmt/format.h>
#include <sstream>

namespace hgps {

namespace {
std::string csv_escape(const std::string &s) {
    if (s.find(',') != std::string::npos || s.find('"') != std::string::npos) {
        std::string out = "\"";
        for (char c : s) {
            if (c == '"')
                out += "\"\"";
            else
                out += c;
        }
        out += '"';
        return out;
    }
    return s;
}
} // namespace

std::filesystem::path
IndividualIDTrackingWriter::make_tracking_path(const std::filesystem::path &base_result_path) {
    auto path_str = base_result_path.string();
    auto dot_pos = path_str.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return path_str.substr(0, dot_pos) + "_IndividualIDTracking.csv";
    }
    return path_str + "_IndividualIDTracking.csv";
}

IndividualIDTrackingWriter::IndividualIDTrackingWriter(
    const std::filesystem::path &base_result_path)
    : stream_{make_tracking_path(base_result_path), std::ofstream::out | std::ofstream::app} {
    if (stream_.fail() || !stream_.is_open()) {
        throw std::invalid_argument(fmt::format("Cannot open IndividualIDTracking file: {}",
                                                make_tracking_path(base_result_path).string()));
    }
}

void IndividualIDTrackingWriter::write_header(const IndividualTrackingEventMessage &message) {
    if (message.rows.empty()) {
        return;
    }
    // Build risk factor column order from first row (sorted for stable headers)
    risk_factor_columns_.clear();
    for (const auto &[k, v] : message.rows.front().risk_factors) {
        (void)v;
        risk_factor_columns_.push_back(k);
    }
    std::sort(risk_factor_columns_.begin(), risk_factor_columns_.end());

    stream_ << "run,time,scenario,id,age,gender,region,ethnicity,income_category";
    for (const auto &col : risk_factor_columns_) {
        stream_ << ',' << csv_escape(col);
    }
    stream_ << '\n';
    stream_.flush();
    header_written_ = true;
}

void IndividualIDTrackingWriter::write_row(unsigned int run, int time, const std::string &scenario,
                                           const IndividualTrackingRow &row,
                                           const std::vector<std::string> &risk_factor_columns) {
    stream_ << run << ',' << time << ',' << csv_escape(scenario) << ',' << row.id << ',' << row.age
            << ',' << csv_escape(row.gender) << ',' << csv_escape(row.region) << ','
            << csv_escape(row.ethnicity) << ',' << csv_escape(row.income_category);
    for (const auto &col : risk_factor_columns) {
        auto it = row.risk_factors.find(col);
        if (it != row.risk_factors.end()) {
            stream_ << ',' << it->second;
        } else {
            stream_ << ',';
        }
    }
    stream_ << '\n';
}

void IndividualIDTrackingWriter::write(const IndividualTrackingEventMessage &message) {
    std::scoped_lock lock(mutex_);
    if (message.rows.empty()) {
        return;
    }
    if (!header_written_) {
        write_header(message);
    }
    for (const auto &row : message.rows) {
        write_row(message.run_number, message.model_time, message.scenario_name, row,
                  risk_factor_columns_);
    }
    // MAHIMA: Flush every 5 writes to reduce I/O while keeping data visible during long runs.
    // Full flush also happens when the writer is destroyed (stream close).
    write_count_++;
    if (write_count_ % 5u == 0u) {
        stream_.flush();
    }
}

} // namespace hgps
