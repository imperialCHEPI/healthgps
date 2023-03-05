#pragma once
#include <chrono>
#include <iostream>

namespace hgps::core {

	/// @brief Timer to printout scope execution time in milliseconds
	class ScopedTimer {
	public:
		/// @brief The time measuring clock
		using ClockType = std::chrono::steady_clock;

		/// @brief Initialise a new instance of the ScopedTimer class.
		/// @param func The scope function name identification
		ScopedTimer(const char* func)
			: function_{ func }, start_{ ClockType::now() }
		{}

		ScopedTimer(const ScopedTimer&) = delete;
		ScopedTimer(ScopedTimer&&) = delete;
		auto operator=(const ScopedTimer&)->ScopedTimer & = delete;
		auto operator=(ScopedTimer&&)->ScopedTimer & = delete;

		/// @brief Destroys the ScopedTimer instance, printout lifetime duration
		~ScopedTimer() {
			using namespace std::chrono;
			auto stop = ClockType::now();
			auto duration = (stop - start_);
			auto ms = duration_cast<milliseconds>(duration).count();
			std::cout << ms << " ms " << function_ << '\n';
		}

	private:
		const char* function_ = {};
		const ClockType::time_point start_ = {};
	};
}
