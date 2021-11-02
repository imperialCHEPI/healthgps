#pragma once

#include "randombit_generator.h"

namespace hgps
{
	/// @brief General purpose random number interface 
	class Random
	{
	public:
		Random() = delete;
		Random(RandomBitGenerator& generator, unsigned int loop_max_trials);
		Random(RandomBitGenerator&& generator, unsigned int loop_max_trials);

		int next_int();

		int next_int(const int& max_value);

		int next_int(const int& min_value, const int& max_value);

		double next_double() noexcept;

		double next_normal();

		double next_normal(double mean, double standard_deviation);

		double next_normal(double mean, double standard_deviation, double boundary);

		int next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf);

		int next_empirical_discrete(const std::vector<int>& values, const std::vector<double>& cdf);

	private:
		RandomBitGenerator& engine_;
		int maximum_trials_;

		double sample_normal_distribution(double mean, double std_dev);
	};
}
