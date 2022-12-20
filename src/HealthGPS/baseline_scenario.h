#pragma once

#include "scenario.h"
#include <functional>

namespace hgps {

	class BaselineScenario final : public Scenario {
	public:
		BaselineScenario() = delete;
		BaselineScenario(SyncChannel& data_sync);

		ScenarioType type() const noexcept override;

		const std::string& name() const noexcept override;

		SyncChannel& channel() override;

		void clear() noexcept override;

		double apply(Random& generator, Person& entity, int time,
			const core::Identifier& risk_factor_key, double value) override;

	private:
		std::reference_wrapper<SyncChannel> channel_;
		std::string name_{ "Baseline" };
	};
}

