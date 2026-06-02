#pragma once

#include <cstdio>
#include <functional>
#include <string>

#include <gtest/gtest.h>

namespace hgps::test {

/// @brief Captures bytes written to the process stdout (including fmt::print output).
/// Always restores stdout even when @p action throws (gtest CaptureStdout requirement).
inline std::string capture_stdout(const std::function<void()> &action) {
    testing::internal::CaptureStdout();
    try {
        action();
    } catch (...) {
        static_cast<void>(testing::internal::GetCapturedStdout());
        std::fflush(stdout);
        throw;
    }
    const std::string output = testing::internal::GetCapturedStdout();
    std::fflush(stdout);
    return output;
}

} // namespace hgps::test
