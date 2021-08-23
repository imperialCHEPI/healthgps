#include "runtime_context.h"
#include <random>

namespace hgps {
    RuntimeContext::RuntimeContext(
        RandomBitGenerator& generator,
        const HierarchicalMapping& mapping,
        const std::vector<core::DiseaseInfo>& diseases,
        const core::IntegerInterval& age_range)
        : generator_{ generator }, mapping_{ mapping }, diseases_{diseases},
        age_range_{ age_range }, time_now_{}, reference_time_{}, population_{ 0 }
    {}

    int RuntimeContext::time_now() const noexcept {
        return time_now_;
    }

    int RuntimeContext::reference_time() const noexcept {
        return reference_time_;
    }

    Population& RuntimeContext::population() noexcept {
        return population_;
    }

    const HierarchicalMapping& RuntimeContext::mapping() const noexcept {
        return mapping_;
    }

    const std::vector<core::DiseaseInfo>& RuntimeContext::diseases() const noexcept {
        return diseases_;
    }

    const core::IntegerInterval& RuntimeContext::age_range() const noexcept {
        return age_range_;
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

    void RuntimeContext::reset_population(const std::size_t pop_size, const int reference_time) {
        population_ = Population{ pop_size };
        reference_time_ = reference_time;
    }
}
