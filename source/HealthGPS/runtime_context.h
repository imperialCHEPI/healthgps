#pragma once

#include <vector>
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
			const std::vector<core::DiseaseInfo>& diseases,
			const core::IntegerInterval& age_range);

		int time_now() const noexcept;

		int reference_time() const noexcept;

		Population& population() noexcept;

		const HierarchicalMapping& mapping() const noexcept;

		const std::vector<core::DiseaseInfo>& diseases() const noexcept;

		const core::IntegerInterval& age_range() const noexcept;

		int next_int();

		int next_int(int max_value);

		int next_int(int min_value, int max_value);

		double next_double() noexcept;

		void set_current_time(const int time_now) noexcept;

		void reset_population(const std::size_t pop_size, const int reference_time);

	private:
		RandomBitGenerator& generator_;
		Population population_;
		HierarchicalMapping mapping_;
		std::vector<core::DiseaseInfo> diseases_;
		core::IntegerInterval age_range_;
		int reference_time_{};
		int time_now_{};
	};
}