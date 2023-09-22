#include "HealthGPS/runtime_metric.h"
#include "pch.h"
#include <thread>

TEST(TestHealthGPS_Metrics, CreateDefault) {
    using namespace hgps;

    auto metrics = RuntimeMetric{};

    ASSERT_TRUE(metrics.empty());
    ASSERT_EQ(0, metrics.size());
    ASSERT_FALSE(metrics.contains("any"));
}

TEST(TestHealthGPS_Metrics, EmplaceMetrics) {
    using namespace hgps;

    auto metrics = RuntimeMetric{};
    metrics.emplace("a", 13.7);
    metrics.emplace("b", 15.3);
    metrics.emplace("c", 3.13);
    for (const auto &m : metrics) {
        ASSERT_GT(m.second, 0.0);
    }

    ASSERT_FALSE(metrics.empty());
    ASSERT_EQ(3, metrics.size());
    ASSERT_TRUE(metrics.contains("a"));
    ASSERT_EQ(3.13, metrics["c"]);
    ASSERT_EQ(metrics["c"], metrics.at("c"));
    for (const auto &m : metrics) {
        ASSERT_TRUE(m.second > 0.0);
    }
}

TEST(TestHealthGPS_Metrics, MetricsReset) {
    using namespace hgps;

    auto metrics = RuntimeMetric{};
    metrics.emplace("a", 13.7);
    metrics.emplace("b", 15.3);
    metrics.emplace("c", 3.13);

    ASSERT_EQ(3, metrics.size());
    for (const auto &m : metrics) {
        ASSERT_TRUE(m.second > 0.0);
    }

    metrics.reset();
    ASSERT_EQ(3, metrics.size());
    for (const auto &m : metrics) {
        ASSERT_TRUE(m.second == 0.0);
    }
}

TEST(TestHealthGPS_Metrics, MetricsClear) {
    using namespace hgps;

    auto metrics = RuntimeMetric{};
    metrics.emplace("a", 13.7);
    metrics.emplace("b", 15.3);
    metrics.emplace("c", 3.13);

    ASSERT_EQ(3, metrics.size());

    metrics.clear();
    ASSERT_TRUE(metrics.empty());
    ASSERT_EQ(0, metrics.size());
    ASSERT_FALSE(metrics.contains("a"));
}
