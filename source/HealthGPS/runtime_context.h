#pragma once

#include "population.h"
#include "mapping.h"

namespace hgps {

	class RuntimeContext
	{
	public:
		RuntimeContext() = delete;
		RuntimeContext(RandomBitGenerator& generator, const HierarchicalMapping& mapping);

		int time_now() const noexcept;

		Population& population() noexcept;

		const HierarchicalMapping& mapping() const noexcept;

		unsigned int next_int() noexcept;

		double next_double() noexcept;

		void set_current_time(const int time_now) noexcept;

		void reset_population(const std::size_t pop_size);

	private:
		RandomBitGenerator& generator_;
		HierarchicalMapping mapping_;
		Population population_;
		int time_now_{};
	};
}