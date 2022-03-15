#pragma once

#include "randombit_generator.h"
#include <functional>

namespace hgps
{
	/// @brief General purpose random number interface 
	class Random
	{
	public:
		Random() = delete;
		Random(RandomBitGenerator& generator);

		int next_int();

		int next_int(const int& max_value);

		int next_int(const int& min_value, const int& max_value);

		double next_double() noexcept;

		double next_normal();

		double next_normal(const double& mean, const double& standard_deviation);

		int next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf);

		int next_empirical_discrete(const std::vector<int>& values, const std::vector<double>& cdf);

	private:
		std::reference_wrapper<RandomBitGenerator> engine_;
		int next_int_internal(const int& min_value, const int& max_value);
		double next_uniform_internal(const double& min_value, const double& max_value);
		double next_normal_internal(const double& mean, const double& standard_deviation);
	};
}
