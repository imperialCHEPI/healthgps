#pragma once

#include <filesystem>

namespace hgps {
//! Get the path to the directory of the currently executing program
std::filesystem::path get_program_directory();

//! Get the path to the currently executing program
std::filesystem::path get_program_path();

//! The cache folder for Health-GPS
std::filesystem::path get_cache_directory();

//! A temporary directory for Health-GPS
std::filesystem::path get_temporary_directory();
} // namespace hgps
