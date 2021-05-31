#include <stdexcept>
#include "scenario.h"

namespace hgps {
	Scenario::Scenario(int start_time, int stop_time) 
		: start_time_(start_time), stop_time_(stop_time)
	{
		if (start_time >= stop_time) {
			throw std::invalid_argument("Stop time must be greater than start time.");
		}
	}

	int hgps::Scenario::get_start_time() { return start_time_; }

	int hgps::Scenario::get_stop_time()	{ return stop_time_; }
}