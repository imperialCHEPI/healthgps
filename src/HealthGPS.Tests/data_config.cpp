#include "data_config.h"

#include <filesystem>
#include <stdexcept>

std::string test_datastore_path;

std::string resolve_path(const std::string &relative_path) {
    namespace fs = std::filesystem;
    auto base_path = fs::current_path();
    while (base_path.has_parent_path()) {
        auto absolute_path = base_path / relative_path;
        if (fs::exists(absolute_path)) {
            return absolute_path.string();
        }

        base_path = base_path.parent_path();
    }

    throw std::runtime_error("Folder not found!");
}

std::string default_datastore_path() {
    if (!test_datastore_path.empty()) {
        return test_datastore_path;
    }

    return resolve_path("data");
}
