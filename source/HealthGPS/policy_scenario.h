#pragma once

#include "scenario.h"
#include <vector>

namespace hgps {
	
	struct PolicyImpact;
	struct PolicyInterval;

	/// @brief Health GPS policy impact types enumeration
	enum class PolicyImpactType : uint8_t
	{
		/// @brief Absolute impact
		absolute,

		/// @brief Relative impact
		relative,
	};

	/// @brief Health GPS intervention policy scenario interface
	class PolicyScenario : public Scenario {
	public:

		virtual const PolicyImpactType& impact_type() const noexcept = 0;

		virtual const PolicyInterval& active_period()  const noexcept = 0;

		virtual const std::vector<PolicyImpact>& impacts() const noexcept = 0;
	};

	/// @brief Defines the policy impact on risk factors data structure
	struct PolicyImpact {
		PolicyImpact() = delete;
		PolicyImpact(std::string risk_factor_key, double policy_impact)
			: risk_factor{ risk_factor_key }, value{ policy_impact } {}

		const std::string risk_factor{};
		const double value{};
	};

	/// @brief Defines the policy active interval
	struct PolicyInterval {
		PolicyInterval(int start_at_time, std::optional<int> finish_at_time = std::nullopt)
			: start_time{ start_at_time }, finish_time{ finish_at_time } {
			if (start_at_time < 0) {
				throw std::out_of_range("Policy start time must not be negative.");
			}

			if (finish_at_time.has_value() && start_at_time >= finish_at_time.value()) {
				throw std::out_of_range("Policy finish time must be greater than the start time.");
			}
		}

		const int start_time{};
		const std::optional<int> finish_time{};
		bool contains(const int& time) const noexcept {
			if (time < start_time) {
				return false;
			}

			return time <= finish_time.value_or(time);
		}
	};
}
