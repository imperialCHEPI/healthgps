#include "modelrunner.h"
#include "runner_message.h"
#include "finally.h"

#include <chrono>
#include <thread>

namespace hgps {

	using ElapsedTime = std::chrono::duration<double, std::milli>;

	ModelRunner::ModelRunner(EventAggregator& bus) noexcept
		: event_bus_{ bus }, running_{ false }, source_{} {}

	double ModelRunner::run(Simulation& baseline, const unsigned int trial_runs)
	{
		if (trial_runs < 1) {
			throw std::invalid_argument("The number of trial runs must not be less than one");
		}
		/*
		else if (baseline.type() != ScenarioType::baseline) {
			throw std::invalid_argument(
				std::format("Simulation: '{}' can not be evaluated alone", baseline.identifier()));
		}
		*/

		if (running_.load()) {
			throw std::invalid_argument("The model runner is already evaluating an experiment");
		}

		source_ = std::stop_source{};
		running_.store(true);
		auto reset = make_finally([this]() {running_.store(false); });

		auto runner_id = "single_runner";
		auto start = std::chrono::steady_clock::now();
		event_bus_.publish_async(std::make_unique<RunnerEventMessage>(runner_id, RunnerAction::start));

		/* Initialise simulation */
		baseline.initialize();

		for (auto run = 1u; run <= trial_runs; run++)
		{
			auto worker = std::jthread(&ModelRunner::run_model_thread, this,
				source_.get_token(), std::ref(baseline), run);

			worker.join();
			if (source_.stop_requested()) {
				event_bus_.publish_async(std::make_unique<RunnerEventMessage>(runner_id, RunnerAction::cancelled));
				break;
			}
		}

		/* Terminate simulation */
		baseline.terminate();

		ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
		auto elapsed_ms = elapsed.count();

		event_bus_.publish_async(std::make_unique<RunnerEventMessage>(runner_id, RunnerAction::finish, elapsed_ms ));
		running_.store(false);
		return elapsed_ms;
	}

	double ModelRunner::run(Simulation& baseline, Simulation& intervention, const unsigned int trial_runs) 
	{
		if (trial_runs < 1) {
			throw std::invalid_argument("The number of trial runs must not be less than one.");
		}
		else if (baseline.type() != ScenarioType::baseline) {
			throw std::invalid_argument(
				std::format("Baseline simulation: {} type mismatch.", baseline.identifier()));
		}
		else if (intervention.type() != ScenarioType::intervention) {
			throw std::invalid_argument(
				std::format("Intervention simulation: {} type mismatch.", intervention.identifier()));
		}

		if (running_.load()) {
			throw std::invalid_argument("The model runner is already evaluating an experiment.");
		}

		running_.store(true);
		auto reset = make_finally([this]() {running_.store(false); });

		auto runner_id = "parallel_runner";
		auto start = std::chrono::steady_clock::now();
		event_bus_.publish_async(std::make_unique<RunnerEventMessage>(runner_id, RunnerAction::start));

		/* Initialise simulation */
		baseline.initialize();
		intervention.initialize();

		for (auto run = 1u; run <= trial_runs; run++)
		{
			// TODO: Sync random bit seed using a master seed generator.
			auto base_worker = std::jthread(&ModelRunner::run_model_thread, this,
				source_.get_token(), std::ref(baseline), run);

			auto policy_worker = std::jthread(&ModelRunner::run_model_thread, this,
				source_.get_token(), std::ref(intervention), run);

			base_worker.join();
			policy_worker.join();
			if (source_.stop_requested()) {
				event_bus_.publish_async(std::make_unique<RunnerEventMessage>(runner_id, RunnerAction::cancelled));
				break;
			}
		}

		/* Terminate simulation */
		baseline.terminate();
		intervention.terminate();

		ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
		auto elapsed_ms = elapsed.count();

		event_bus_.publish_async(std::make_unique<RunnerEventMessage>(runner_id, RunnerAction::finish, elapsed_ms));
		return elapsed_ms;
	}

	bool ModelRunner::is_running() const noexcept {
		return running_.load();
	}

	void ModelRunner::cancel() noexcept {
		if (is_running()) {
			source_.request_stop();
		}
	}

	void ModelRunner::run_model_thread(std::stop_token token, Simulation& model, const unsigned int run)
	{
		auto run_start = std::chrono::steady_clock::now();
		event_bus_.publish_async(std::make_unique<RunnerEventMessage>(
			model.identifier(), RunnerAction::run_begin, run));

		/* Create the simulation engine */
		adevs::Simulator<int> sim;

		/* Update model and add to engine */
		model.set_current_run(run);
		sim.add(&model);

		/* Run until the next event is at infinity */
		while (!token.stop_requested() && sim.next_event_time() < adevs_inf<adevs::Time>() ) {
			sim.exec_next_event();
		}

		ElapsedTime elapsed = std::chrono::steady_clock::now() - run_start;
		event_bus_.publish_async(std::make_unique<RunnerEventMessage>(
			model.identifier(), RunnerAction::run_end, run, elapsed.count()));
	}
}
