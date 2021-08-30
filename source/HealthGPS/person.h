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
		Free,

		/// @brief current with the condition
		Active
	};

	struct Disease
	{
		DiseaseStatus status;
		int start_time;
	};

	struct Person
	{
		Person();
		Person(const core::Gender gender) noexcept;

		const size_t id() const noexcept;

		core::Gender gender{ core::Gender::unknown };

		unsigned int age{};

		bool is_alive{ true };

		unsigned int time_of_death{};

		TwoStepValue<unsigned int> education{};

		TwoStepValue<unsigned int> income{};

		std::map<std::string, double> risk_factors;

		std::map<std::string, Disease> diseases;

		bool is_active() const noexcept;

		double get_risk_factor_value(const std::string& key) const noexcept;

		float gender_to_value() const noexcept;

		static void reset_id();

	private:
		size_t id_;
		static std::atomic<size_t> newUID;
		static case_insensitive_map<std::function<double(const Person&)>> dispatcher;
	};
}

