#pragma once

#include <random>
#include "interfaces.h"

namespace hgps {

	class MTRandom32 final : public RandomBitGenerator
	{
	public:
		MTRandom32();
		explicit MTRandom32(const unsigned int seed);

		unsigned int operator()() override;

		void seed(const unsigned int) override;

		void discard(const unsigned long long skip) override;

		int next_int() override; 

		int next_int(const int& max_value) override;

		int next_int(const int& min_value, const int& max_value) override;

		double next_double() noexcept override;

		int next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf) override;

		int next_empirical_discrete(const std::vector<int>& values, const std::vector<double>& cdf) override;

		static constexpr unsigned int min() { return std::mt19937::min(); }
		static constexpr unsigned int max() { return std::mt19937::max(); }

	private:
		std::mt19937 engine_;
	};
}
