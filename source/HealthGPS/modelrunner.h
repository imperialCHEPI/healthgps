#pragma once
#include <atomic>
#include <stop_token>
#include "simulation.h"
#include "event_aggregator.h"

namespace hgps {

	class ModelRunner
	{
	public:
		ModelRunner() = delete;
		ModelRunner(EventAggregator& bus) noexcept;

		double run(Simulation& baseline, const unsigned int trial_runs);

		double run(Simulation& baseline, Simulation& intervention, const unsigned int trial_runs);
		
		bool is_running() const noexcept;

		void cancel() noexcept;

	private:
		std::atomic<bool> running_;
		EventAggregator& event_bus_;
		std::stop_source source_;

		void run_model_thread(std::stop_token token, Simulation& model, const unsigned int run);
	};
}