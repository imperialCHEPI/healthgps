#include "mtrandom.h"

namespace hgps {

	// -----------------------------------------------------------------------
	// Implement Mersenne Twister 19937 (32 bit) random bit generator 


	MTRandom32::MTRandom32() {
		std::random_device rd;
		engine_.seed(rd());
	}

	MTRandom32::MTRandom32(const unsigned int seed)	{
		engine_.seed(seed);
	}

	unsigned int MTRandom32::operator()() {
		return engine_();
	}

	void MTRandom32::seed(const unsigned int seed) {
		engine_.seed(seed);
	}

	void MTRandom32::discard(const unsigned long long skip) { 
		engine_.discard(skip);
	}

	int MTRandom32::next_int() {
		return next_int(0, std::numeric_limits<int>::max());
	}

	int MTRandom32::next_int(const int& max_value) {
		return next_int(0, max_value);
	}

	int MTRandom32::next_int(const int& min_value, const int& max_value) {
		if (min_value < 0 || max_value < min_value) {
			throw std::invalid_argument(
				"min_value must be non-negative, and less than or equal to max_value.");
		}

		std::uniform_int_distribution<int> distribution(min_value, max_value);
		return distribution(engine_);
	}

	double MTRandom32::next_double() noexcept {
		return std::generate_canonical<double, std::numeric_limits<double>::digits>(engine_);
	}

	int MTRandom32::next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf) {
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