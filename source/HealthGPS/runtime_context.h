#pragma once

#include "population.h"

namespace hgps {

	class RuntimeContext
	{
	public:
		RuntimeContext() = delete;
		RuntimeContext(RandomBitGenerator& generator);

		int time_now() const noexcept;

		const Population& population() const noexcept;

		unsigned int next_int() noexcept;

		double next_double() noexcept;

		void set_current_time(const int time_now) noexcept;

		void reset_population(const std::size_t pop_size);

	private:
		RandomBitGenerator& generator_;
		Population population_;
		int time_now_{};
	};
}