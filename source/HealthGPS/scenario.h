#pragma once

#include <string>
#include <memory>

#include "channel.h"
#include "sync_message.h"
#include "person.h"

namespace hgps {

	/// @brief Scenario data synchronisation channel type
	using SyncChannel = Channel<std::unique_ptr<SyncMessage>>;

	/// @brief Health GPS policy scenario types enumeration
	enum class ScenarioType : uint8_t
	{
		/// @brief Baseline scenario
		baseline,

		/// @brief Intervention scenario
		intervention,
	};

	/// @brief Health GPS simulation scenario interface
	class Scenario {
	public:
		virtual ~Scenario() = default;

		virtual ScenarioType type() const noexcept = 0;

		virtual std::string name() const noexcept = 0;

		virtual SyncChannel& channel() = 0;

		virtual void clear() noexcept = 0;

		virtual double apply(Person& entity, const int time,
			const std::string risk_factor_key, const double value) = 0;
	};
}