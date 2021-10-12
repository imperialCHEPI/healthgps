#pragma once

#include <limits>
#include <vector>

namespace hgps {

	/// @brief Random number generator algorithms interface
	class RandomBitGenerator {
	public:
		using result_type = unsigned int;

		virtual ~RandomBitGenerator() = default;

		virtual unsigned int operator()() = 0;

		virtual void seed(const unsigned int seed) = 0;

		virtual void discard(const unsigned long long skip) = 0;

		virtual int next_int() = 0;

		virtual int next_int(const int& max_value) = 0;

		virtual int next_int(const int& min_value, const int& max_value) = 0;

		virtual double next_double() noexcept = 0;

		virtual int next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf) = 0;

		virtual int next_empirical_discrete(const std::vector<int>& values, const std::vector<double>& cdf) = 0;

		static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
		static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
	};
}
