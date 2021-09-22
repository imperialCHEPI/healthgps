#include "runtime_context.h"
#include <random>

namespace hgps {

    RuntimeContext::RuntimeContext(EventAggregator& bus, SimulationDefinition& definition)
        : event_bus_{ bus }, definition_{ definition }, population_{ 0 }
    {}

    int RuntimeContext::time_now() const noexcept {
        return time_now_;
    }

    int RuntimeContext::start_time() const noexcept {
        return model_start_time_;
    }

    unsigned int RuntimeContext::current_run() const noexcept {
        return current_run_;
    }

    Population& RuntimeContext::population() noexcept {
        return population_;
    }

    RuntimeMetric& RuntimeContext::metrics() noexcept {
        return metrics_;
    }

    const HierarchicalMapping& RuntimeContext::mapping() const noexcept {
        return definition_.inputs().risk_mapping();
    }

    const std::vector<core::DiseaseInfo>& RuntimeContext::diseases() const noexcept {
        return definition_.inputs().diseases();
    }

    const core::IntegerInterval& RuntimeContext::age_range() const noexcept {
        return definition_.inputs().settings().age_range();
    }

    const std::string RuntimeContext::identifier() const noexcept {
        return definition_.identifier();
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
        return distribution(definition_.rnd());
    }

    double RuntimeContext::next_double() noexcept {
        return definition_.rnd().next_double();
    }

    int RuntimeContext::next_empirical_discrete(const std::vector<int>& values, const std::vector<float>& cdf) {
        if (values.size() != cdf.size()) {
            throw std::invalid_argument(
                std::format("input vectors size mismatch: {} vs {}.", values.size(), cdf.size()));
        }

        auto p = definition_.rnd().next_double();
        for (size_t i = 0; i < cdf.size(); i++) {
            if (p <= cdf[i]) {
                return values[i];
            }
        }

        return values.back();
    }

    void RuntimeContext::set_current_time(const int time_now) noexcept {
        time_now_ = time_now;
    }

    void RuntimeContext::set_current_run(const unsigned int run_number) noexcept {
        current_run_ = run_number;
    }

    void RuntimeContext::reset_population(const std::size_t initial_pop_size, const int model_start_time) {
        population_ = Population{ initial_pop_size };
        model_start_time_ = model_start_time;
    }

    void RuntimeContext::publish(std::unique_ptr<EventMessage> message) const noexcept {
        event_bus_.publish(std::move(message));
    }

    void RuntimeContext::publish_async(std::unique_ptr<EventMessage> message) const noexcept {
        event_bus_.publish_async(std::move(message));
    }
}
