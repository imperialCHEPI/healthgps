#include "schema.h"

#include <fmt/format.h>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

#include <fstream>

namespace hgps::data {
using namespace jsoncons;

json resolve_uri(const jsoncons::uri &uri, const std::filesystem::path &schema_directory) {
    const auto &uri_str = uri.string();
    if (!uri_str.starts_with(HGPS_SCHEMA_URL_PREFIX)) {
        throw std::runtime_error(fmt::format("Unable to load URL: {}", uri_str));
    }

    // Strip URL prefix and load file from local filesystem
    const auto filename = std::filesystem::path{uri.path()}.filename();
    const auto schema_path = schema_directory / filename;
    auto ifs = std::ifstream{schema_path};
    if (!ifs) {
        throw std::runtime_error("Failed to read schema file");
    }

    return json::parse(ifs);
}

void validate_index(const std::filesystem::path &schema_directory, std::istream &index_stream) {
    // **YUCK**: We have to read in the data with jsoncons here rather than reusing
    // the nlohmann-json representation :-(
    const auto index = json::parse(index_stream);

    // Load schema
    auto ifs_schema = std::ifstream{schema_directory / "data_index.json"};
    if (!ifs_schema) {
        throw std::runtime_error("Failed to load schema");
    }

    const auto resolver = [&schema_directory](const auto &uri) {
        return resolve_uri(uri, schema_directory);
    };
    const auto schema = jsonschema::make_json_schema(json::parse(ifs_schema), resolver);

    // Perform validation
    schema.validate(index);
}
} // namespace hgps::data
