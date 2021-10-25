#include "random_algorithm.h"

#include <random>
#include <format>
#include <stdexcept>

namespace hgps
{
	Random::Random(RandomBitGenerator& generator)
		: engine_{generator} { }

	Random::Random(RandomBitGenerator&& generator)
		:  engine_{generator} {}

	int Random::next_int() {
		return next_int(0, std::numeric_limits<int>::max());
	}

	int Random::next_int(const int& max_value) {
		return next_int(0, max_value);
	}

	int Random::next_int(const int& min_value, const int& max_value) {
		if (min_value < 0 || max_value < min_value) {
			throw std::invalid_argument(
				"min_value must be non-negative, and less than or equal to max_value.");
		}

		std::uniform_int_distribution<int> distribution(min_value, max_value);
		return distribution(engine_);
	}

	double Random::next_double() noexcept {
		return engine_.next_double();
	}

	double Random::next_normal() {
		return next_normal(0.0, 1.0);
	}

	double Random::next_normal(double mean, double standard_deviation) {
		auto gaussian = std::normal_distribution(mean, standard_deviation);
		return gaussian(engine_);
	}

	int Random::next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf)
	{
		if (values.size() != cdf.size()) {
			throw std::invalid_argument(
				std::format("input vectors size mismatch: {} vs {}.", values.size(), cdf.size()));
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
				std::format("input vectors size mismatch: {} vs {}.", values.size(), cdf.size()));
		}

		auto p = next_double();
		for (size_t i = 0; i < cdf.size(); i++) {
			if (p <= cdf[i]) {
				return values[i];
			}
		}

		return values.back();
	}
}
