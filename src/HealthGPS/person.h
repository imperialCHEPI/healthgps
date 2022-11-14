#pragma once

#include <map>
#include <atomic>

#include "interfaces.h"
#include "HealthGPS.Core/identifier.h"

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
		DiseaseStatus status{};
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

		std::map<core::Identifier, double> risk_factors;

		std::map<core::Identifier, Disease> diseases;

		bool is_alive() const noexcept;

		bool has_emigrated() const noexcept;

		unsigned int time_of_death() const noexcept;

		unsigned int time_of_migration() const noexcept;

		bool is_active() const noexcept;

		double get_risk_factor_value(const core::Identifier& key) const noexcept;

		float gender_to_value() const noexcept;

		std::string gender_to_string() const noexcept;

		void emigrate(const unsigned int time);

		void die(const unsigned int time);

		static void reset_id();

	private:
		std::size_t id_{};
		bool is_alive_{ true };
		bool has_emigrated_{ false };
		unsigned int time_of_death_{};
		unsigned int time_of_migration_{};

		static std::atomic<std::size_t> newUID;
		static std::map<core::Identifier, std::function<double(const Person&)>> current_dispatcher;
	};
}

