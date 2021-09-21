#include "baseline_scenario.h"

namespace hgps {

    BaselineScenario::BaselineScenario(SyncChannel& data_sync)
        : channel_{data_sync} {}

    ScenarioType BaselineScenario::type() const noexcept {
        return ScenarioType::baseline;
    }

    std::string BaselineScenario::name() const noexcept {
        return "Baseline";
    }

    SyncChannel& BaselineScenario::channel() {
        return channel_;
    }

    double BaselineScenario::apply(const int& time, const std::string& risk_factor_key, const double& value) {
        return value;
    }
}
