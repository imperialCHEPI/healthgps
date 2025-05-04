#include "baseline_scenario.h"
#include <iostream>

namespace hgps {

BaselineScenario::BaselineScenario(SyncChannel &data_sync) : channel_{data_sync} {}

void BaselineScenario::clear() noexcept {}

SyncChannel &BaselineScenario::channel() { return channel_.get(); }

double BaselineScenario::apply([[maybe_unused]] Random &generator, [[maybe_unused]] Person &entity,
                               [[maybe_unused]] int time,
                               [[maybe_unused]] const core::Identifier &risk_factor_key,
                               [[maybe_unused]] double value) {
    return value;
}
} // namespace hgps
