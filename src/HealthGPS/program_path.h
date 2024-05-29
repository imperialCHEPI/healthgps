#pragma once

#include <filesystem>

namespace hgps {
//! Get the path to the directory of the currently executing program
std::filesystem::path get_program_directory();

//! Get the path to the currently executing program
std::filesystem::path get_program_path();
} // namespace hgps