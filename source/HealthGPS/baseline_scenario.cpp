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

    void BaselineScenario::clear() noexcept {
    }

    double BaselineScenario::apply([[maybe_unused]] Person& entity, [[maybe_unused]] const int& time,
        [[maybe_unused]] const std::string& risk_factor_key, [[maybe_unused]] const double& value) {
        return value;
    }
}
