#include "schema.h"

#include "HealthGPS/program_dirs.h"

#include <fmt/format.h>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

#include <fstream>

namespace {
using namespace jsoncons;

//! The prefix for Health-GPS schema URLs
static constexpr const char *SchemaURLPrefix =
    "https://raw.githubusercontent.com/imperialCHEPI/healthgps/main/schemas/";

json resolve_uri(const uri &uri, const std::filesystem::path &program_directory) {
    const auto &uri_str = uri.string();
    if (!uri_str.starts_with(SchemaURLPrefix)) {
        throw std::runtime_error(fmt::format("Unable to load URL: {}", uri_str));
    }

    // Strip URL prefix and load file from local filesystem
    const auto uri_path = std::filesystem::path{uri.path()};
    const auto version_string = uri_path.parent_path().filename();
    const auto schema_file_name = uri_path.filename();
    const auto schema_path = program_directory / "schemas" / version_string / schema_file_name;
    auto ifs = std::ifstream{schema_path};
    if (!ifs) {
        throw std::runtime_error("Failed to read schema file");
    }

    return json::parse(ifs);
}

/// @brief Validate a JSON file against the specified schema
/// @param json_stream The input stream for the JSON file
/// @param schema_file_name The name of the JSON schema file
/// @param schema_version The version of the schema file
void validate_json(std::istream &json_stream, const char *schema_file_name, int schema_version) {
    // **YUCK**: We have to read in the data with jsoncons here rather than reusing the
    // nlohmann-json representation :-(
    const auto data = json::parse(json_stream);

    // Load schema
    const auto program_dir = hgps::get_program_directory();
    const auto schema_relative_path =
        std::filesystem::path{"schemas"} / fmt::format("v{}", schema_version) / schema_file_name;
    auto ifs_schema = std::ifstream{program_dir / schema_relative_path};
    if (!ifs_schema) {
        throw std::runtime_error("Failed to load schema");
    }

    const auto resolver = [&program_dir](const auto &uri) { return resolve_uri(uri, program_dir); };
    const auto schema = jsonschema::make_json_schema(json::parse(ifs_schema), resolver);

    // Perform validation
    schema.validate(data);
}
} // anonymous namespace

namespace hgps::input {
nlohmann::json load_and_validate_json(const std::filesystem::path &file_path,
                                      const char *schema_file_name, int schema_version) {
    auto ifs = std::ifstream{file_path};
    if (!ifs) {
        throw std::runtime_error(fmt::format("File not found: {}", file_path.string()));
    }

    // Read in JSON file
    auto json = nlohmann::json::parse(ifs);

    // Check that the file has a $schema property and that it matches the URL of the
    // schema version we support
    if (!json.contains("$schema")) {
        throw std::runtime_error(
            fmt::format("File missing required $schema property: {}", file_path.string()));
    }

    // Check $schema attribute is present and valid
    const auto actual_schema_url = json.at("$schema").get<std::string>();
    const auto expected_schema_url =
        fmt::format("{}v{}/{}", SchemaURLPrefix, schema_version, schema_file_name);
    if (actual_schema_url != expected_schema_url) {
        throw std::runtime_error(fmt::format("Invalid schema URL provided: {} (expected: {})",
                                             actual_schema_url, expected_schema_url));
    }

    // Perform validation
    ifs.seekg(0); // Seek to start of file so we can reload
    validate_json(ifs, schema_file_name, schema_version);

    return json;
}
} // namespace hgps::input
