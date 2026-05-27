#pragma once

#include <cstdio>
#include <functional>
#include <string>

#include <gtest/gtest.h>

namespace hgps::test {

/// @brief Captures bytes written to the process stdout (including fmt::print output).
inline std::string capture_stdout(const std::function<void()> &action) {
    testing::internal::CaptureStdout();
    action();
    const std::string output = testing::internal::GetCapturedStdout();
    std::fflush(stdout);
    return output;
}

} // namespace hgps::test
