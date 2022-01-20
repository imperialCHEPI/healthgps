#pragma once

#include <map>
#include <atomic>

#include "interfaces.h"
#include "two_step_value.h"

namespace hgps {

	/// @brief Disease status enumeration
	enum struct DiseaseStatus : uint8_t
	{
		/// @brief declared free from condition
		free,

		/// @brief current with the condition
		active
	};

	struct Disease
	{
		DiseaseStatus status;
		int start_time{};
		int time_since_onset{ -1 };

		Disease clone() const noexcept {
			return Disease{ .status = status, .start_time = start_time, .time_since_onset = time_since_onset };
		}
	};

	struct Person
	{
		Person();
		Person(const core::Gender gender) noexcept;

		std::size_t id() const noexcept;

		core::Gender gender{ core::Gender::unknown };

		unsigned int age{};

		double ses{};

		bool is_alive{ true };

		bool has_emigrated{ false };

		unsigned int time_of_death{};

		TwoStepValue<int> education{};

		TwoStepValue<int> income{};

		std::map<std::string, double> risk_factors;

		std::map<std::string, Disease> diseases;

		bool is_active() const noexcept;

		double get_risk_factor_value(const std::string& key) const noexcept;

		double get_previous_risk_factor_value(const std::string& key) const noexcept;

		float gender_to_value() const noexcept;

		static void reset_id();

	private:
		std::size_t id_{};
		static std::atomic<std::size_t> newUID;
		static case_insensitive_map<std::function<double(const Person&)>> current_dispatcher;
		static case_insensitive_map<std::function<double(const Person&)>> previous_dispatcher;
	};
}

