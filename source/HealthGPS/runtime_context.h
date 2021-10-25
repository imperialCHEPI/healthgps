#pragma once

#include <vector>
#include "population.h"
#include "mapping.h"
#include "event_aggregator.h"
#include "runtime_metric.h"
#include "simulation_definition.h"
#include "random_algorithm.h"

namespace hgps {

	class RuntimeContext
	{
	public:
		RuntimeContext() = delete;
		RuntimeContext(EventAggregator& bus, SimulationDefinition& definition);

		int time_now() const noexcept;

		int start_time() const noexcept;

		unsigned int current_run() const noexcept;

		int sync_timeout_millis() const noexcept;

		Population& population() noexcept;

		RuntimeMetric& metrics() noexcept;

		Scenario& scenario() noexcept;

		Random& random() noexcept;

		const HierarchicalMapping& mapping() const noexcept;

		const std::vector<core::DiseaseInfo>& diseases() const noexcept;

		const core::IntegerInterval& age_range() const noexcept;

		const std::string identifier() const noexcept;

		void set_current_time(const int time_now) noexcept;

		void set_current_run(const unsigned int run_number) noexcept;

		void reset_population(const std::size_t initial_pop_size, const int model_start_time);

		void publish(std::unique_ptr<EventMessage> message) const noexcept;

		void publish_async(std::unique_ptr<EventMessage> message) const noexcept;

	private:
		Random generator_;
		Population population_;
		EventAggregator& event_bus_;
		SimulationDefinition& definition_;
		RuntimeMetric metrics_{};
		unsigned int current_run_{};
		int model_start_time_{};
		int time_now_{};
	};
}