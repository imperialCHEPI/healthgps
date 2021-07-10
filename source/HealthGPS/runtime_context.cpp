#include "runtime_context.h"
#include <random>

namespace hgps {
    RuntimeContext::RuntimeContext(RandomBitGenerator& generator, const HierarchicalMapping& mapping)
        : generator_{ generator }, mapping_{ mapping }, time_now_{}, population_{ 0 }
    {}

    int RuntimeContext::time_now() const noexcept {
        return time_now_;
    }

    Population& RuntimeContext::population() noexcept {
        return population_;
    }

    const HierarchicalMapping& RuntimeContext::mapping() const noexcept {
        return mapping_;
    }

    int RuntimeContext::next_int() {
        return next_int(0, std::numeric_limits<int>::max());
    }

    int RuntimeContext::next_int(int max_value) {
        return next_int(0, max_value);
    }

    int RuntimeContext::next_int(int min_value, int max_value) {
        if (min_value < 0 ||  max_value < min_value) {
            throw std::invalid_argument(
                "min_value must be non-negative, and less than or equal to max_value.");
        }

        std::uniform_int_distribution<int> distribution(min_value, max_value);
        return distribution(generator_);
    }

    double RuntimeContext::next_double() noexcept {
        return generator_.next_double();
    }

    void RuntimeContext::set_current_time(const int time_now) noexcept {
        time_now_ = time_now;
    }

    void RuntimeContext::reset_population(const std::size_t pop_size) {
        population_ = Population{ pop_size };
    }
}
