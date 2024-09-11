#pragma once

#include "data_source.h"

#include <filesystem>
#include <string>

namespace hgps::input {
/// @brief A data source which is validated with a checksum (unless it's a directory)
class ValidatedDataSource : public DataSource {
  public:
    /// @brief Create a new ValidatedDataSource
    /// @details Note that the file hash is not required for directories, but is compulsory
    ///          otherwise
    /// @param source File/directory path or URL
    /// @param root_path Path to use as root for relative paths
    /// @param file_hash SHA256 hash of file contents
    explicit ValidatedDataSource(std::string source, const std::filesystem::path &root_path,
                                 std::string file_hash);

    ~ValidatedDataSource() override = default;

    /// @brief Compute the checksum for the file
    /// @param zip_file_path Path to zip file
    /// @return The calculated checksum
    std::string compute_file_hash(const std::filesystem::path &zip_file_path) const override;

  private:
    std::string file_hash_;
};
} // namespace hgps::input
