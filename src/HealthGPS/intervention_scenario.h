#pragma once

#include "scenario.h"
#include <vector>

namespace hgps {

	struct PolicyImpact;
	struct PolicyInterval;

	/// @brief Health GPS intervention policy scenario interface
	class InterventionScenario : public Scenario {
	public:
		virtual const PolicyInterval& active_period()  const noexcept = 0;

		virtual const std::vector<PolicyImpact>& impacts() const noexcept = 0;
	};

	/// @brief Defines the policy impact on risk factors data structure
	struct PolicyImpact {
		PolicyImpact() = delete;
		PolicyImpact(std::string risk_factor_key, double policy_impact, 
			const unsigned int start_age, std::optional<unsigned int> end_age = std::nullopt)
			: risk_factor{ risk_factor_key }, value{ policy_impact },
			from_age{ start_age }, to_age{ end_age } {

			if (end_age.has_value() && start_age > end_age.value()) {
				throw std::out_of_range("Impact end age must be equal or greater than the start age.");
			}
		}

		const std::string risk_factor{};
		const double value{};
		const unsigned int from_age{};
		const std::optional<unsigned int> to_age{};
		bool contains(const unsigned int& age) const noexcept {
			if (age < from_age) {
				return false;
			}

			if (to_age.has_value()) {
				return age <= to_age.value();
			}

			return true;
		}
	};

	/// @brief Defines the policy active interval
	struct PolicyInterval {
		PolicyInterval(int start_at_time, std::optional<int> finish_at_time = std::nullopt)
			: start_time{ start_at_time }, finish_time{ finish_at_time } {
			if (start_at_time < 0) {
				throw std::out_of_range("Policy start time must not be negative.");
			}

			if (finish_at_time.has_value() && start_at_time > finish_at_time.value()) {
				throw std::out_of_range("Policy finish time must be equal or greater than the start time.");
			}
		}

		const int start_time{};
		const std::optional<int> finish_time{};
		bool contains(const int& time) const noexcept {
			if (time < start_time) {
				return false;
			}

			if (finish_time.has_value()) {
				return time <= finish_time.value();
			}

			return true;
		}
	};
}
