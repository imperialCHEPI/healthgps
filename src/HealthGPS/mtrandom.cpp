#include "mtrandom.h"

namespace hgps {

// -----------------------------------------------------------------------
// Implement Mersenne Twister 19937 (32 bit) random bit generator

MTRandom32::MTRandom32() {
    std::random_device rd;
    engine_.seed(rd());
}

MTRandom32::MTRandom32(const unsigned int seed) { engine_.seed(seed); }

void MTRandom32::seed(const unsigned int seed) { engine_.seed(seed); }

void MTRandom32::discard(const unsigned long long skip) { engine_.discard(skip); }

unsigned int MTRandom32::next() { return engine_(); }

double MTRandom32::next_double() noexcept {
    return std::generate_canonical<double, std::numeric_limits<double>::digits>(engine_);
}
} // namespace hgps
