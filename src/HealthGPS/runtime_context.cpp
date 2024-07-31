#include "runtime_context.h"
#include "HealthGPS/mtrandom.h"

namespace hgps {

RuntimeContext::RuntimeContext(EventAggregator &bus,
                               std::unique_ptr<SimulationDefinition> definition)
    : event_bus_{bus}, definition_{std::move(definition)}, population_{0} {}

int RuntimeContext::time_now() const noexcept { return time_now_; }

int RuntimeContext::start_time() const noexcept { return model_start_time_; }

unsigned int RuntimeContext::current_run() const noexcept { return current_run_; }

int RuntimeContext::sync_timeout_millis() const noexcept {
    return definition_->inputs().sync_timeout_ms();
}

SimulationDefinition &RuntimeContext::definition() noexcept { return *definition_; }

const SimulationDefinition &RuntimeContext::definition() const noexcept { return *definition_; }

Population &RuntimeContext::population() noexcept { return population_; }

const Population &RuntimeContext::population() const noexcept { return population_; }

RuntimeMetric &RuntimeContext::metrics() noexcept { return metrics_; }

Scenario &RuntimeContext::scenario() noexcept { return definition_->scenario(); }

Random &RuntimeContext::random() const noexcept { return random_; }

const HierarchicalMapping &RuntimeContext::mapping() const noexcept {
    return definition_->inputs().risk_mapping();
}

const std::vector<core::DiseaseInfo> &RuntimeContext::diseases() const noexcept {
    return definition_->inputs().diseases();
}

const core::IntegerInterval &RuntimeContext::age_range() const noexcept {
    return definition_->inputs().settings().age_range();
}

std::string RuntimeContext::identifier() const noexcept { return definition_->identifier(); }

void RuntimeContext::set_current_time(const int time_now) noexcept { time_now_ = time_now; }

void RuntimeContext::set_current_run(const unsigned int run_number) noexcept {
    current_run_ = run_number;
}

void RuntimeContext::reset_population(const std::size_t initial_pop_size) {
    population_ = Population{initial_pop_size};
    model_start_time_ = definition_->inputs().start_time();
}

void RuntimeContext::publish(std::unique_ptr<EventMessage> message) const noexcept {
    event_bus_.get().publish(std::move(message));
}

void RuntimeContext::publish_async(std::unique_ptr<EventMessage> message) const noexcept {
    event_bus_.get().publish_async(std::move(message));
}
} // namespace hgps
