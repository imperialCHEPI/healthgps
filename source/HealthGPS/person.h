#pragma once

#include <map>
#include <atomic>

#include "interfaces.h"

namespace hgps {

	struct Person
	{
		Person();

		const size_t id() const noexcept;

		core::Gender gender{ core::Gender::unknown };

		unsigned int age{};

		bool is_alive{ true };

		unsigned int time_of_death{};

		unsigned int education{};

		unsigned int income{};

		case_insensitive_map<double> risk_factors;

		std::map<std::string, int> diseases;

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

