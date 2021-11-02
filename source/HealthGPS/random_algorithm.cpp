#include "random_algorithm.h"

#include <random>
#include <numbers>
#include <format>
#include <stdexcept>

namespace hgps
{
	Random::Random(RandomBitGenerator& generator, unsigned int loop_max_trials)
		: engine_{ generator }, maximum_trials_{ static_cast<int>(loop_max_trials) } {}

	Random::Random(RandomBitGenerator&& generator, unsigned int loop_max_trials)
		: engine_{ generator }, maximum_trials_{ static_cast<int>(loop_max_trials) } {}

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

	double Random::next_normal(double mean, double standard_deviation, double boundary) {
		auto gaussian = std::normal_distribution(mean, standard_deviation);
		auto absolute_boundary = std::abs(boundary);
		for (auto i = 0; i < maximum_trials_; i++) {
			auto candidate = gaussian(engine_);
			if (candidate + absolute_boundary >= 0.0) {
				return candidate;
			}
		}

		throw std::out_of_range(
			std::format("Maximum trials: {} reached without satisfying the bound condition.",
				maximum_trials_));
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

	double Random::sample_normal_distribution(double mean, double std_dev)
	{
		// Uniform(0,1] random doubles
		double u1 = 1.0 - engine_.next_double();
		double u2 = 1.0 - engine_.next_double();

		// Random normal(0,1)
		double std_normal = std::sqrt(-2.0 * std::log(u1)) * std::sin(2.0 * std::numbers::pi * u2);

		// Random normal(mean, std_dev^2 )
		return mean + std_dev * std_normal;
	}
}
