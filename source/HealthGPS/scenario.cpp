#include <stdexcept>
#include "scenario.h"

namespace hgps {
	Scenario::Scenario(int start_time, int stop_time, int trial_runs)
		: start_time_(start_time), stop_time_(stop_time), trial_runs_{trial_runs}
	{
		if (start_time >= stop_time) {
			throw std::invalid_argument("Stop time must be greater than start time.");
		}

		if (trial_runs_ < 1) {
			throw std::invalid_argument("The number of trial runs must not be less than one.");
		}
	}

	int hgps::Scenario::get_start_time() { return start_time_; }

	int hgps::Scenario::get_stop_time()	{ return stop_time_; }
}