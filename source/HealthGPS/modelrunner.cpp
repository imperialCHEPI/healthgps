#include <chrono>
#include "modelrunner.h"

namespace hgps {
	ModelRunner::ModelRunner(adevs::Model<int>& model, SimulationModuleFactory& factory)
		: simulation_{ model }, factory_{ factory }
	{
	}

	double ModelRunner::run() const
	{
		auto start = std::chrono::steady_clock::now();

		/* Create the simulation context */
		adevs::Simulator<int> sim;

		/* Add the model to the context */
		sim.add(&simulation_);

		/* Run until the next event is at infinity */
		while (sim.next_event_time() < adevs_inf<adevs::Time>()) {
			sim.exec_next_event();
		}

		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
		return elapsed.count();
	}
}
