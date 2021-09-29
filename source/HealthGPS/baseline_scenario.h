#pragma once

#include "scenario.h"

namespace hgps {

	class BaselineScenario final : public Scenario {
	public:
		BaselineScenario() = delete;
		BaselineScenario(SyncChannel& data_sync);

		ScenarioType type() const noexcept override;

		std::string name() const noexcept override;

		SyncChannel& channel() override;

		double apply(const int& time, const std::string& risk_factor_key, const double& value) override;

	private:
		SyncChannel& channel_;
	};
}

