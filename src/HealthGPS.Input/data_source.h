#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace hgps::input {

//! Represents a source (either file/directory path or a URL) for static data
class DataSource {
  public:
    /// @brief Create a new DataSource without checksum validatation
    /// @param source File/directory path or URL
    explicit DataSource(std::string source);

    /// @brief Create a new DataSource with checksum validation
    /// @details Note that the file hash is not required for directories, but is compulsory
    ///          otherwise
    /// @param source File/directory path or URL
    /// @param root_path Path to use as root for relative paths
    /// @param file_hash SHA256 hash of file contents
    explicit DataSource(std::string source, const std::filesystem::path &root_path,
                        std::optional<std::string> file_hash);

    // Copy and move constructors
    DataSource(const DataSource &) noexcept = default;
    DataSource(DataSource &&) noexcept = default;
    DataSource &operator=(const DataSource &) noexcept = default;
    DataSource &operator=(DataSource &&) noexcept = default;

    /// @brief Get the path to a directory containing the data
    /// @details This function will download, extract and validate data as needed
    /// @return Path to directory containing the data
    std::filesystem::path get_data_directory() const;

  private:
    std::string source_;
    std::optional<std::string> file_hash_;
    bool validate_checksum_;
};
} // namespace hgps::input
