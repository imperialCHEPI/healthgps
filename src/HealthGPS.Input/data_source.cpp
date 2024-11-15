#include "data_source.h"
#include "HealthGPS/sha256.h"
#include "download_file.h"
#include "zip_file.h"

#include <fmt/format.h>

namespace {
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

// If source is a relative path, rebase it on root_path, else just return source
std::string try_rebase_path(std::string source, const std::filesystem::path &root_path) {
    // If source is a URL, leave it alone
    if (source.starts_with("http://") || source.starts_with("https://")) {
        return source;
    }

    const std::filesystem::path path = source;
    if (path.is_absolute()) {
        return source;
    }

    return std::filesystem::absolute(root_path / path).string();
}
} // anonymous namespace

namespace hgps::input {

DataSource::DataSource(std::string source) : source_(std::move(source)) {}

DataSource::DataSource(std::string source, const std::filesystem::path &root_path)
    : source_(try_rebase_path(std::move(source), root_path)) {}

std::string DataSource::compute_file_hash(const std::filesystem::path &zip_file_path) const {
    return hgps::compute_sha256_for_file(zip_file_path);
}

std::filesystem::path DataSource::get_data_directory() const {
    if (std::filesystem::is_directory(source_)) {
        return source_;
    }

    const auto zip_file_path = get_zip_file_path(source_);
    const auto file_hash = compute_file_hash(zip_file_path);

    // If the cache folder already exists, then we don't need to extract anything
    auto cache_path = hgps::input::get_zip_cache_directory(file_hash);
    if (std::filesystem::is_directory(cache_path)) {
        return cache_path;
    }

    // Extract files
    hgps::input::extract_zip_file(zip_file_path, cache_path);

    return cache_path;
}
} // namespace hgps::input
