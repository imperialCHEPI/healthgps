#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace hgps::input {

//! Represents a data source, a either file/directory path or a URL
class DataSource {
  public:
    /// @brief Create a new DataSource
    /// @param source File/directory path or URL
    explicit DataSource(std::string source);

    /// @brief Create a new DataSource, rebasing relative paths if needed
    /// @param source File path or URL
    /// @param root_path Path to use as root for relative paths
    explicit DataSource(std::string source, const std::filesystem::path &root_path);

    virtual ~DataSource() = default;

    // Copy and move constructors
    DataSource(const DataSource &) noexcept = default;
    DataSource(DataSource &&) noexcept = default;
    DataSource &operator=(const DataSource &) noexcept = default;
    DataSource &operator=(DataSource &&) noexcept = default;

    /// @brief Compute the checksum for the file
    /// @param zip_file_path Path to zip file
    /// @return The calculated checksum
    virtual std::string compute_file_hash(const std::filesystem::path &zip_file_path) const;

    /// @brief Get the path to a directory containing the data
    /// @details This function will download, extract and validate data as needed
    /// @return Path to directory containing the data
    std::filesystem::path get_data_directory() const;

  protected:
    std::string source_;
};
} // namespace hgps::input
