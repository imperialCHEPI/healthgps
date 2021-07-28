#pragma once

#include "population.h"
#include "mapping.h"

namespace hgps {

	class RuntimeContext
	{
	public:
		RuntimeContext() = delete;
		RuntimeContext(
			RandomBitGenerator& generator,
			const HierarchicalMapping& mapping,
			const core::IntegerInterval& age_range);

		int time_now() const noexcept;

		Population& population() noexcept;

		const HierarchicalMapping& mapping() const noexcept;

		const core::IntegerInterval& age_range() const noexcept;

		int next_int();

		int next_int(int max_value);

		int next_int(int min_value, int max_value);

		double next_double() noexcept;

		void set_current_time(const int time_now) noexcept;

		void reset_population(const std::size_t pop_size);

	private:
		RandomBitGenerator& generator_;
		HierarchicalMapping mapping_;
		Population population_;
		core::IntegerInterval age_range_;
		int time_now_{};
	};
}