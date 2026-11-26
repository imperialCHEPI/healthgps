#pragma once

#include <cstdio>
#include <functional>
#include <string>

#include <gtest/gtest.h>
#include <oneapi/tbb/global_control.h>

namespace hgps::test {

/// @brief Captures bytes written to the process stdout (including fmt::print output).
/// Always restores stdout even when @p action throws (gtest CaptureStdout requirement).
inline std::string capture_stdout(const std::function<void()> &action) {
    // MAHIMA: gtest redirects fd 1. oneTBB 2023 workers then abort or never return,
    // which fails the Kevin Hall weight tests. One thread writes the same tables.
    // Production parallelism is unchanged: this limit lives only around the capture.
    const tbb::global_control single_thread(tbb::global_control::max_allowed_parallelism, 1);
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
