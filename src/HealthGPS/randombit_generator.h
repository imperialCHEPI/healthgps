#pragma once

#include <limits>
#include <vector>

namespace hgps {

	/// @brief Random number generator algorithms interface
	class RandomBitGenerator {
	public:
		using result_type = unsigned int;

		/// @brief Destroys a RandomBitGenerator instance
		virtual ~RandomBitGenerator() = default;

		/// @brief Generates the next random number
		/// @return A pseudo-random number value in [\c min(), \c max()]
		virtual unsigned int operator()() = 0;

		/// @brief Sets the current state of the generator engine
		/// @param seed The value to initialise the internal state
		virtual void seed(const unsigned int seed) = 0;

		/// @brief Advances the engine's state by a specified amount
		/// @param skip The number of times to advance the state
		virtual void discard(const unsigned long long skip) = 0;

		/// @brief Generates a random floating point number in range [0,1)
		/// @return A floating point value in range [0,1).
		virtual double next_double() noexcept = 0;

		/// @brief Gets the smallest possible value in the output range
		/// @return The minimum potentially generated value.
		static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }

		/// @brief Gets the largest possible value in the output range
		/// @return The maximum potentially generated value.
		static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
	};
}
