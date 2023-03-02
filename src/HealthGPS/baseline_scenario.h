#pragma once

#include "scenario.h"
#include <functional>

namespace hgps {

	/// @brief Defines the baseline scenario data type
	/// @details The baseline scenario is a special case, 
	/// which identify the type as ScenarioType::baseline and
	/// when applied to a Person makes no changes to any risk
	/// factor value. This implementation is final.
	class BaselineScenario final : public Scenario {
	public:
		BaselineScenario() = delete;
		/// @brief Initialises a new instance of the BaselineScenario class.
		/// @param data_sync The data synchronisation channel instance to use.
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

