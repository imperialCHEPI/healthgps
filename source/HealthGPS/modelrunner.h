#pragma once
#include "simulation.h"
#include "event_aggregator.h"

#include <atomic>
#include <stop_token>
#include <memory>
#include <functional>

namespace hgps {

	class ModelRunner
	{
	public:
		ModelRunner() = delete;
		ModelRunner(EventAggregator& bus, std::unique_ptr<RandomBitGenerator> generator) noexcept;

		double run(Simulation& baseline, const unsigned int trial_runs);

		double run(Simulation& baseline, Simulation& intervention, const unsigned int trial_runs);
		
		bool is_running() const noexcept;

		void cancel() noexcept;

	private:
		std::atomic<bool> running_;
		std::reference_wrapper<EventAggregator> event_bus_;
		std::unique_ptr<RandomBitGenerator> rnd_;
		std::stop_source source_;
		std::string runner_id_{};

		void run_model_thread(std::stop_token token, Simulation& model,
			const unsigned int run, const std::optional<unsigned int> seed = std::nullopt);

		void notify(std::unique_ptr<hgps::EventMessage> message);
	};
}