#include "runtime_context.h"

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

    unsigned int RuntimeContext::next_int() noexcept {
        return generator_();
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
