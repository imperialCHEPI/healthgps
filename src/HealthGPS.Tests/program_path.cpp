#include "program_path.h"

#ifdef __linux__
#include <climits>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "HealthGPS.Core/exception.h"

std::filesystem::path get_program_directory() { return get_program_path().parent_path(); }

std::filesystem::path get_program_path() {
    bool success;

#if defined(__linux__)
    char path[PATH_MAX + 1];
    ssize_t len = readlink("/proc/self/exe", path, PATH_MAX);
    if (!(success = (len >= 0))) {
        path[len] = '\0';
    }
#elif defined(_WIN32)
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    success = (len > 0);
#elif defined(__APPLE__)
    char path[MAXPATHLEN + 1];
    uint32_t len = sizeof(path);
    success = (_NSGetExecutablePath(path, &len) == 0);
#else
#error "Unsupported platform"
#endif

    if (!success) {
        throw hgps::core::HgpsException("Could not get program path");
    }

    return std::filesystem::path{path};
}
