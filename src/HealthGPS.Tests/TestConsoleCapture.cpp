#include "TestConsoleCapture.h"

#include <cstdio>

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
        ::testing::UnitTest::GetInstance()->listeners().Append(new FlushStdoutListener);
    }
};

const RegisterFlushStdoutListener k_register_flush_stdout_listener{};

} // namespace

} // namespace hgps::test
