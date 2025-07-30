#pragma once
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "model_info.h"
#include "result_writer.h"

namespace hgps {
/// @brief Defines the results message file stream writer class
///
/// The message content are written to a JSON (JavaScript Object Notation) file,
/// experiment information and global averages values, while `time-series` data
/// is written to a associated CSV (Comma-separated Values) file with same name
/// but different extension.
class ResultFileWriter final : public ResultWriter {
  public:
    ResultFileWriter() = delete;
    /// @brief Initialises an instance of the hgps::ResultFileWriter class.
    /// @param file_name The JSON output file full name
    /// @param info The associated experiment information
    ResultFileWriter(const std::filesystem::path &file_name, ExperimentInfo info);

    ResultFileWriter(const ResultFileWriter &) = delete;
    ResultFileWriter &operator=(const ResultFileWriter &) = delete;
    ResultFileWriter(ResultFileWriter &&other) noexcept;
    ResultFileWriter &operator=(ResultFileWriter &&other) noexcept;

    /// @brief Destroys a hgps::ResultFileWriter instance
    ~ResultFileWriter();
    void write(const hgps::ResultEventMessage &message) override;

  private:
    std::ofstream stream_;
    std::ofstream csvstream_;
    std::mutex lock_mutex_;
    std::atomic<bool> first_row_{true};
    ExperimentInfo info_;
    std::filesystem::path base_filename_;

    void write_json_begin(const std::filesystem::path &output);
    void write_json_end();

    static std::string to_json_string(const hgps::ResultEventMessage &message);
    void write_csv_channels(const hgps::ResultEventMessage &message);
    void write_csv_header(const hgps::ResultEventMessage &message);
    
    /// @brief Writes income-based CSV files for each income category
    /// @param message The result event message containing income-based data
    void write_income_based_csv_files(const hgps::ResultEventMessage &message);
    
    /// @brief Writes income-specific CSV data for a given income category
    /// @param message The result event message
    /// @param income The income category
    /// @param income_csv The output file stream for this income category
    void write_income_csv_data(const hgps::ResultEventMessage &message, 
                               core::Income income, std::ofstream &income_csv);
    
    /// @brief Generates filename for income-based CSV files
    /// @param base_filename The base filename
    /// @param income The income category
    /// @return The income-specific filename
    std::string generate_income_filename(const std::string &base_filename, core::Income income) const;
    
    /// @brief Converts income category enum to string representation
    /// @param income The income category
    /// @return String representation of the income category
    std::string income_category_to_string(core::Income income) const;
    
    /// @brief Gets available income categories from the result message
    /// @param message The result event message
    /// @return Vector of available income categories
    std::vector<core::Income> get_available_income_categories(const hgps::ResultEventMessage &message) const;
};
} // namespace hgps
