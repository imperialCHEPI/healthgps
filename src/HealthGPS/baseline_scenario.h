#pragma once

#include "scenario.h"
#include <functional>

namespace hgps {

	class BaselineScenario final : public Scenario {
	public:
		BaselineScenario() = delete;
		BaselineScenario(SyncChannel& data_sync);

		ScenarioType type() const noexcept override;

		std::string name() const noexcept override;

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Person& entity, const int time,
			const std::string risk_factor_key, const double value) override;

	private:
		std::reference_wrapper<SyncChannel> channel_;
	};
}

