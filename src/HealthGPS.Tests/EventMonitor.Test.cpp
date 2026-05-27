#include "pch.h"

#include "HealthGPS.Console/event_monitor.h"
#include "HealthGPS/event_bus.h"
#include "HealthGPS/result_message.h"

#include <gtest/gtest.h>

namespace {

class CountingResultWriter final : public hgps::ResultWriter {
  public:
    void write(const hgps::ResultEventMessage &message) override {
        ++write_count_;
        last_time_ = message.model_time;
        last_source_ = message.source;
    }

    int write_count() const { return write_count_; }
    int last_time() const { return last_time_; }
    const std::string &last_source() const { return last_source_; }

  private:
    int write_count_{0};
    int last_time_{0};
    std::string last_source_;
};

} // namespace

TEST(EventMonitor, ResultEventsAreWrittenSynchronouslyOnPublish) {
    using namespace hgps;

    hgps::DefaultEventBus bus;
    CountingResultWriter writer;

    {
        EventMonitor monitor(bus, writer);

        ModelResult first(1u);
        bus.publish(std::make_unique<ResultEventMessage>("baseline", 1u, 2020, std::move(first)));
        EXPECT_EQ(1, writer.write_count());
        EXPECT_EQ(2020, writer.last_time());

        ModelResult second(1u);
        bus.publish(
            std::make_unique<ResultEventMessage>("intervention", 2u, 2021, std::move(second)));
        EXPECT_EQ(2, writer.write_count());
        EXPECT_EQ(2021, writer.last_time());
        EXPECT_EQ("intervention", writer.last_source());
    }
}
