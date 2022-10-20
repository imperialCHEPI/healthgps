#pragma once

#include <string>
extern std::string test_datastore_path;

std::string resolve_path(const std::string& relative_path);

std::string default_datastore_path();
