#pragma once

#include "HealthGPS/individual_tracking_message.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

namespace hgps {

/// MAHIMA: Writes per-person rows to the IndividualIDTracking CSV only (no JSON). Same person (by id)
/// can be verified across baseline and intervention. Filename: base result path with suffix
/// _IndividualIDTracking.csv.
///
/// Output columns: run, time, scenario, id, age, gender, region, ethnicity, income_category,
/// then one column per risk factor (from config.risk_factors or all from mapping). To add more
/// columns: extend IndividualTrackingRow, fill in analysis_module, and update write_header/write_row.
class IndividualIDTrackingWriter {
  public:
    IndividualIDTrackingWriter() = delete;

    /// @param base_result_path Full path of the main result file (e.g. HealthGPS_result_<timestamp>.json)
    explicit IndividualIDTrackingWriter(const std::filesystem::path &base_result_path);

    IndividualIDTrackingWriter(const IndividualIDTrackingWriter &) = delete;
    IndividualIDTrackingWriter &operator=(const IndividualIDTrackingWriter &) = delete;
    IndividualIDTrackingWriter(IndividualIDTrackingWriter &&) = default;
    IndividualIDTrackingWriter &operator=(IndividualIDTrackingWriter &&) = default;

    void write(const IndividualTrackingEventMessage &message);

  private:
    static std::filesystem::path make_tracking_path(const std::filesystem::path &base_result_path);
    void write_header(const IndividualTrackingEventMessage &message);
    void write_row(unsigned int run, int time, const std::string &scenario,
                   const IndividualTrackingRow &row,
                   const std::vector<std::string> &risk_factor_columns);

    std::ofstream stream_;
    std::mutex mutex_;
    bool header_written_{false};
    std::vector<std::string> risk_factor_columns_;
    unsigned int write_count_{0};  // MAHIMA: for periodic flush (every 5 writes)
};

} // namespace hgps
