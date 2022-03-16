#include "random_algorithm.h"

#include <random>
#include <numbers>
#include <stdexcept>
#include <fmt/format.h>

namespace hgps
{
	Random::Random(RandomBitGenerator& generator)
		: engine_{ generator } {}

	int Random::next_int() {
		return next_int(0, std::numeric_limits<int>::max()-1);
	}

	int Random::next_int(const int& max_value) {
		return next_int(0, max_value);
	}

	int Random::next_int(const int& min_value, const int& max_value) {
		if (min_value < 0 || max_value < min_value) {
			throw std::invalid_argument(
				"min_value must be non-negative, and less than or equal to max_value.");
		}

		//std::uniform_int_distribution<int> distribution(min_value, max_value);
		//return distribution(engine_.get());

		return next_int_internal(min_value, max_value);
	}

	double Random::next_double() noexcept {
		return engine_.get().next_double();
	}

	double Random::next_normal() {
		return next_normal(0.0, 1.0);
	}

	double Random::next_normal(const double& mean, const double& standard_deviation) {
		if (standard_deviation <= 0.0) {
			throw std::invalid_argument("The standard deviation parameter must be greater than zero");
		}

		// auto gaussian = std::normal_distribution(mean, standard_deviation);
		// return gaussian(engine_);

		return next_normal_internal(mean, standard_deviation);
	}

	int Random::next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf)
	{
		if (values.size() != cdf.size()) {
			throw std::invalid_argument(
				fmt::format("input vectors size mismatch: {} vs {}.", values.size(), cdf.size()));
		}

		auto p = next_double();
		for (size_t i = 0; i < cdf.size(); i++) {
			if (p <= cdf[i]) {
				return values[i];
			}
		}

		return values.back();
	}

	int  Random::next_empirical_discrete(const std::vector<int>& values, const std::vector<double>& cdf) {
		if (values.size() != cdf.size()) {
			throw std::invalid_argument(
				fmt::format("input vectors size mismatch: {} vs {}.", values.size(), cdf.size()));
		}

		auto p = next_double();
		for (size_t i = 0; i < cdf.size(); i++) {
			if (p <= cdf[i]) {
				return values[i];
			}
		}

		return values.back();
	}

	int Random::next_int_internal(const int& min_value, const int& max_value) {
		return min_value + static_cast<int>((max_value - min_value + 1) * next_double());
	}

	double Random::next_uniform_internal(const double& min_value, const double& max_value) {
		return min_value + (max_value - min_value) * next_double();
	}

	double Random::next_normal_internal(const double& mean, const double& standard_deviation) {
		double p, p1, p2;
		do {
			p1 = next_uniform_internal(-1.0, 1.0);
			p2 = next_uniform_internal(-1.0, 1.0);
			p = p1 * p1 + p2 * p2;
		} while (p >= 1.0);

		return mean + standard_deviation * p1 * std::sqrt(-2.0 * std::log(p) / p);
	}
}
