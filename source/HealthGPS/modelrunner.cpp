#include <chrono>
#include "modelrunner.h"

namespace hgps {
	ModelRunner::ModelRunner(Simulation& model, SimulationModuleFactory& factory, const int trial_runs)
		: simulation_{ model }, factory_{ factory }, trial_runs_{ trial_runs }
	{
		if (trial_runs_ < 1) {
			throw std::invalid_argument("The number of trial runs must not be less than one.");
		}
	}

	double ModelRunner::run() const
	{
		auto start = std::chrono::steady_clock::now();

		/* Initialise simulation */
		simulation_.initialize();

		for (size_t run = 1; run <= trial_runs_; run++)
		{
			std::cout << std::format("Run # {} started...\n", run);

			/* Create the simulation engine */
			adevs::Simulator<int> sim;

			/* Add the model to the engine */
			sim.add(&simulation_);

			/* Run until the next event is at infinity */
			while (sim.next_event_time() < adevs_inf<adevs::Time>()) {
				sim.exec_next_event();
			}

			std::cout << std::format("Run # {} finished...\n", run);
		}

		/* Terminate simulation */
		simulation_.terminate();

		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
		return elapsed.count();
	}
}
