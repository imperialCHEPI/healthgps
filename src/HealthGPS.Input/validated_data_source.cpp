#include "validated_data_source.h"

#include "HealthGPS/sha256.h"

#include <fmt/format.h>

#include <stdexcept>

namespace hgps::input {
ValidatedDataSource::ValidatedDataSource(std::string source, const std::filesystem::path &root_path,
                                         std::string file_hash)
    : DataSource(std::move(source), root_path), file_hash_(std::move(file_hash)) {
    // Sanity check
    if (std::filesystem::is_directory(source_)) {
        throw std::runtime_error("ValidatedDataSource cannot be created with directories.");
    }
}

std::string
ValidatedDataSource::compute_file_hash(const std::filesystem::path &zip_file_path) const {
    // Validate file with checksum
    auto computed_hash = hgps::compute_sha256_for_file(zip_file_path);
    if (computed_hash != file_hash_) {
        throw std::runtime_error(
            fmt::format("Checksum validation failed for {} (actual: {}, expected: {})", source_,
                        computed_hash, file_hash_));
    }

    return computed_hash;
}
} // namespace hgps::input
