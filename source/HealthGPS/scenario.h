#pragma once

#include<optional>

namespace hgps {

	class Scenario
	{

	public:
		Scenario() = delete;
		explicit Scenario(int start_time, int stop_time, int trial_runs);

		std::optional<unsigned int> custom_seed;

		std::string country;

		int get_start_time();
		int get_stop_time();

	private:
		int start_time_;
		int stop_time_;
		int trial_runs_;
	};
}