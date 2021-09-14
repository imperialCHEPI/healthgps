#include <chrono>
#include "modelrunner.h"
#include <HealthGPS/runner_message.h>

namespace hgps {

	using ElapsedTime = std::chrono::duration<double, std::milli>;

	ModelRunner::ModelRunner(EventAggregator& bus) noexcept
		: event_bus_{bus} {}

	double ModelRunner::run(Simulation& model, const unsigned int trial_runs) const
	{
		if (trial_runs < 1) {
			throw std::invalid_argument("The number of trial runs must not be less than one.");
		}

		auto start = std::chrono::steady_clock::now();
		event_bus_.publish_async(std::make_unique<RunnerEventMessage>(model.name(), RunnerAction::start));

		/* Initialise simulation */
		model.initialize();

		for (auto run = 1u; run <= trial_runs; run++)
		{
			auto run_start = std::chrono::steady_clock::now();
			event_bus_.publish_async(std::make_unique<RunnerEventMessage>(
				model.name(), RunnerAction::run_begin, run ));

			/* Create the simulation engine */
			adevs::Simulator<int> sim;

			/* Update model and add to engine */
			model.set_current_run(run);
			sim.add(&model);

			/* Run until the next event is at infinity */
			while (sim.next_event_time() < adevs_inf<adevs::Time>()) {
				sim.exec_next_event();
			}

			ElapsedTime elapsed = std::chrono::steady_clock::now() - run_start;
			event_bus_.publish_async(std::make_unique<RunnerEventMessage>(
				model.name(), RunnerAction::run_end, run, elapsed.count()));
		}

		/* Terminate simulation */
		model.terminate();

		ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
		auto elapsed_ms = elapsed.count();

		event_bus_.publish_async(std::make_unique<RunnerEventMessage>(
			model.name(), RunnerAction::finish, elapsed_ms ));
		return elapsed_ms;
	}
}
