#include "TestConsoleCapture.h"

#include <cstdio>
#include <memory>

namespace hgps::test {

namespace {

class FlushStdoutListener : public ::testing::EmptyTestEventListener {
  public:
    void OnTestEnd(const ::testing::TestInfo & /*test_info*/) override {
        std::fflush(stdout);
        std::fflush(stderr);
    }
};

struct RegisterFlushStdoutListener {
    RegisterFlushStdoutListener() {
        auto listener = std::make_unique<FlushStdoutListener>();
        // gtest::TestEventListeners::Append takes ownership; freed when UnitTest tears down.
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        ::testing::UnitTest::GetInstance()->listeners().Append(listener.release());
    }
};

const RegisterFlushStdoutListener k_register_flush_stdout_listener{};

} // namespace

} // namespace hgps::test
