#include <chrono>
#include "modelrunner.h"

namespace hgps {

	ModelRunner::ModelRunner(SimulationModuleFactory& factory, ModelInput& config)
		: factory_{factory}, config_{config} {}

	double ModelRunner::run(Simulation& model, const unsigned int trial_runs) const
	{
		if (trial_runs < 1) {
			throw std::invalid_argument("The number of trial runs must not be less than one.");
		}

		auto start = std::chrono::steady_clock::now();

		/* Initialise simulation */
		model.initialize();

		for (size_t run = 1; run <= trial_runs; run++)
		{
			std::cout << std::format("Run # {} started...\n", run);

			/* Create the simulation engine */
			adevs::Simulator<int> sim;

			/* Add the model to the engine */
			sim.add(&model);

			/* Run until the next event is at infinity */
			while (sim.next_event_time() < adevs_inf<adevs::Time>()) {
				sim.exec_next_event();
			}

			std::cout << std::format("Run # {} finished...\n", run);
		}

		/* Terminate simulation */
		model.terminate();

		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
		return elapsed.count();
	}
}
