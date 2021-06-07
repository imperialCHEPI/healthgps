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

		double next_double() override;

		void seed(const unsigned int) override;

		void discard(const unsigned long long skip) override;

		static constexpr unsigned int min() { return std::mt19937::min(); }
		static constexpr unsigned int max() { return std::mt19937::max(); }

	private:
		std::mt19937 engine_;
	};
}
