#include "program_dirs.h"

#ifdef __linux__
#include <climits>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

#include "HealthGPS.Core/exception.h"

#include <array>

namespace {
void throw_path_error() { throw hgps::core::HgpsException("Could not get program path"); }
} // anonymous namespace

namespace hgps {
std::filesystem::path get_program_directory() { return get_program_path().parent_path(); }

std::filesystem::path get_program_path() {
#if defined(__linux__)
    std::array<char, PATH_MAX> path{};
    if (readlink("/proc/self/exe", path.data(), path.size() - 1) == -1) {
        throw_path_error();
    }
#elif defined(_WIN32)
    std::array<wchar_t, MAX_PATH> path{};
    if (GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())) == 0) {
        throw_path_error();
    }
#else
#error "Unsupported platform"
#endif

    return path.data();
}
} // namespace hgps
