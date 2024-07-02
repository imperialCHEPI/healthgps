#include "data_source.h"
#include "HealthGPS/sha256.h"
#include "download_file.h"
#include "zip_file.h"

#include <fmt/format.h>

namespace {
// If source is a relative path to a directory, rebase it on root_path, else just return source
std::string try_rebase_path(std::string source, const std::filesystem::path &root_path) {
    if (!std::filesystem::is_directory(source)) {
        return source;
    }

    const std::filesystem::path path = source;
    if (path.is_absolute()) {
        return source;
    }

    return (root_path / path).string();
}

// Get a path to a zip file; if source is a URL it will be downloaded first
std::filesystem::path get_zip_file_path(const std::string &source) {
    if (source.ends_with(".zip") && std::filesystem::is_regular_file(source)) {
        return source;
    }

    // If it's URL rather than a zip file, we have to download it first
    if (source.starts_with("http://") || source.starts_with("https://")) {
        return hgps::input::download_file_to_temporary(source, ".zip");
    }

    throw std::runtime_error(
        "Data source must be a directory, a zip file or a URL pointing to a zip file");
}

std::filesystem::path get_data_directory_with_validation(const std::string &source,
                                                         const std::string &file_hash) {
    // If the cache folder already exists, then we don't need to download or extract anything
    auto cache_path = hgps::input::get_zip_cache_directory(file_hash);
    if (std::filesystem::is_directory(cache_path)) {
        return cache_path;
    }

    const auto zip_file_path = get_zip_file_path(source);

    // Validate file with checksum
    const auto computed_hash = hgps::compute_sha256_for_file(zip_file_path);
    if (computed_hash != file_hash) {
        throw std::runtime_error(
            fmt::format("Checksum validation failed for {} (actual: {}, expected: {})", source,
                        computed_hash, file_hash));
    }

    // Extract files
    hgps::input::extract_zip_file(zip_file_path, cache_path);

    return cache_path;
}

std::filesystem::path get_data_directory_without_validation(const std::string &source) {
    const auto zip_file_path = get_zip_file_path(source);
    const auto file_hash = hgps::compute_sha256_for_file(zip_file_path);

    // If the cache folder already exists, then we don't need to extract anything
    auto cache_path = hgps::input::get_zip_cache_directory(file_hash);
    if (std::filesystem::is_directory(cache_path)) {
        return cache_path;
    }

    // Extract files
    hgps::input::extract_zip_file(zip_file_path, cache_path);

    return cache_path;
}

} // anonymous namespace

namespace hgps::input {

DataSource::DataSource(std::string source)
    : source_(std::move(source)), file_hash_(std::nullopt), validate_checksum_(false) {}

DataSource::DataSource(std::string source, const std::filesystem::path &root_path,
                       std::optional<std::string> file_hash)
    : source_(try_rebase_path(std::move(source), root_path)), file_hash_(std::move(file_hash)),
      validate_checksum_(true) {}

std::filesystem::path DataSource::get_data_directory() const {
    // If the data source is already a directory we can just return it (no checksum validation
    // needed)
    if (std::filesystem::is_directory(source_)) {
        return source_;
    }

    if (!validate_checksum_) {
        return get_data_directory_without_validation(source_);
    }

    if (file_hash_) {
        return get_data_directory_with_validation(source_, *file_hash_);
    }

    throw std::runtime_error("Checksum must be supplied if data source is URL or zip file");
}
} // namespace hgps::input
