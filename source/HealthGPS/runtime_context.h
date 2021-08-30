#pragma once

#include <vector>
#include "population.h"
#include "mapping.h"
#include "event_aggregator.h"

namespace hgps {

	class RuntimeContext
	{
	public:
		RuntimeContext() = delete;
		RuntimeContext(
			EventAggregator& bus,
			RandomBitGenerator& generator,
			const HierarchicalMapping& mapping,
			const std::vector<core::DiseaseInfo>& diseases,
			const core::IntegerInterval& age_range);

		int time_now() const noexcept;

		int reference_time() const noexcept;

		unsigned int current_run() const noexcept;

		Population& population() noexcept;

		const HierarchicalMapping& mapping() const noexcept;

		const std::vector<core::DiseaseInfo>& diseases() const noexcept;

		const core::IntegerInterval& age_range() const noexcept;

		const std::string identifier() const noexcept;

		int next_int();
		int next_int(int max_value);
		int next_int(int min_value, int max_value);
		double next_double() noexcept;
		int next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf);

		void set_current_time(const int time_now) noexcept;

		void set_current_run(const unsigned int run_number) noexcept;

		void reset_population(const std::size_t pop_size, const int reference_time);

		void publish(const EventMessage& message) const noexcept;

		void publish_async(const EventMessage& message) const noexcept;

	private:
		EventAggregator& event_bus_;
		RandomBitGenerator& generator_;
		Population population_;
		HierarchicalMapping mapping_;
		std::vector<core::DiseaseInfo> diseases_;
		core::IntegerInterval age_range_;
		unsigned int current_run_{};
		int reference_time_{};
		int time_now_{};
	};
}