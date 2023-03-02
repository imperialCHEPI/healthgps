#pragma once

#include <random>
#include "randombit_generator.h"

namespace hgps {

	/// @brief Mersenne Twister random number generator algorithm
	class MTRandom32 final : public RandomBitGenerator
	{
	public:
		/// @brief Initialise a new instance of the MTRandom32 class
		MTRandom32();

		/// @brief Initialise a new instance of the MTRandom32 class
		/// @param seed The value to initialise the internal state
		explicit MTRandom32(const unsigned int seed);

		unsigned int operator()() override;

		void seed(const unsigned int) override;

		void discard(const unsigned long long skip) override;

		double next_double() noexcept override;

		/// @copydoc RandomBitGenerator::min
		static constexpr unsigned int min() { return std::mt19937::min(); }

		/// @copydoc RandomBitGenerator::max
		static constexpr unsigned int max() { return std::mt19937::max(); }

	private:
		std::mt19937 engine_;
	};
}
