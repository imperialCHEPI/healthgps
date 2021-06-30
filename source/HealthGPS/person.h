#pragma once

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

		bool is_active() const noexcept;

		static void reset_id();

	private:
		size_t id_;
		static std::atomic<size_t> newUID;
	};
}

