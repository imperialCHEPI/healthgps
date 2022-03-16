#pragma once

#include <random>
#include "randombit_generator.h"

namespace hgps {

	class MTRandom32 final : public RandomBitGenerator
	{
	public:
		MTRandom32();
		explicit MTRandom32(const unsigned int seed);

		unsigned int operator()() override;

		void seed(const unsigned int) override;

		void discard(const unsigned long long skip) override;

		double next_double() noexcept override;

		static constexpr unsigned int min() { return std::mt19937::min(); }
		static constexpr unsigned int max() { return std::mt19937::max(); }

	private:
		std::mt19937 engine_;
	};
}
