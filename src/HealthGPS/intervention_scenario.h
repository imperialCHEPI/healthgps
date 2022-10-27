#pragma once
#include "scenario.h"

#include <optional>
#include <vector>

namespace hgps {

	struct PolicyImpact;
	struct PolicyInterval;
	struct PolicyDynamic;

	/// @brief Health GPS scripted intervention policy scenario interface
	class InterventionScenario : public Scenario {
	public:
		virtual const PolicyInterval& active_period()  const noexcept = 0;

		virtual const std::vector<PolicyImpact>& impacts() const noexcept = 0;
	};

	/// @brief Health GPS dynamic intervention policy scenario interface
	class DynamicInterventionScenario : public InterventionScenario {
	public:
		virtual const PolicyDynamic& dynamic()  const noexcept = 0;
	};

	/// @brief Defines the policy impact on risk factors data structure
	struct PolicyImpact {
		PolicyImpact() = delete;
		PolicyImpact(core::Identifier risk_factor_key, double policy_impact, 
			unsigned int start_age, std::optional<unsigned int> end_age = std::nullopt)
			: risk_factor{ std::move(risk_factor_key) }, value{policy_impact},
			from_age{ start_age }, to_age{ end_age } {

			if (end_age.has_value() && start_age > end_age.value()) {
				throw std::out_of_range("Impact end age must be equal or greater than the start age.");
			}
		}

		core::Identifier risk_factor{};
		double value{};
		unsigned int from_age{};
		std::optional<unsigned int> to_age{};
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

		int start_time{};
		std::optional<int> finish_time{};
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

	/// @brief Defines the policy dynamic parameters
	struct PolicyDynamic
	{
		PolicyDynamic(const std::vector<double>& parameters) {
			if (parameters.size() != size_t{ 3 }) {
				throw std::invalid_argument("The vector of dynamic arguments must have size = 3 [alpha, beta, gamma]");
			}

			for (const auto& p : parameters) {
				if (0.0 > p || p > 1.0) {
					throw std::invalid_argument("Dynamic argument: " + std::to_string(p) + ", outside range [0.0, 1.0]");
				}
			}

			alpha = parameters.at(0);
			beta = parameters.at(1);
			gamma = parameters.at(2);
		}

		double alpha{ 1.0 };
		double beta{ 0.0 };
		double gamma{ 0.0 };
	};
}
