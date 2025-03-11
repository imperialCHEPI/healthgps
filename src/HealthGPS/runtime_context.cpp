#include "runtime_context.h"

namespace {
// Hash function for RegionKey tuple
struct RegionKeyHash {
    std::size_t operator()(const std::tuple<int, hgps::core::Gender> &key) const {
        auto [age, gender] = key;
        return std::hash<int>()(age) ^ std::hash<int>()(static_cast<int>(gender));
    }
};

// Hash function for EthnicityKey tuple
struct EthnicityKeyHash {
    std::size_t
    operator()(const std::tuple<int, hgps::core::Gender, hgps::core::Region> &key) const {
        auto [age, gender, region] = key;
        return std::hash<int>()(age) ^ std::hash<int>()(static_cast<int>(gender)) ^
               std::hash<int>()(static_cast<int>(region));
    }
};
} // namespace

namespace hgps {

RuntimeContext::RuntimeContext(std::shared_ptr<const EventAggregator> bus,
                               std::shared_ptr<const ModelInput> inputs,
                               std::unique_ptr<Scenario> scenario)
    : event_bus_{std::move(bus)}, inputs_{std::move(inputs)}, scenario_{std::move(scenario)},
      population_{0}, age_range_{inputs_->settings().age_range()} {}

int RuntimeContext::time_now() const noexcept { return time_now_; }

int RuntimeContext::start_time() const noexcept { return model_start_time_; }

unsigned int RuntimeContext::current_run() const noexcept { return current_run_; }

int RuntimeContext::sync_timeout_millis() const noexcept { return inputs_->sync_timeout_ms(); }

Population &RuntimeContext::population() noexcept { return population_; }

const Population &RuntimeContext::population() const noexcept { return population_; }

RuntimeMetric &RuntimeContext::metrics() noexcept { return metrics_; }

const ModelInput &RuntimeContext::inputs() const noexcept { return *inputs_; }

Scenario &RuntimeContext::scenario() const noexcept { return *scenario_; }

Random &RuntimeContext::random() const noexcept { return random_; }

const HierarchicalMapping &RuntimeContext::mapping() const noexcept {
    return inputs_->risk_mapping();
}

const std::vector<core::DiseaseInfo> &RuntimeContext::diseases() const noexcept {
    return inputs_->diseases();
}

const core::IntegerInterval &RuntimeContext::age_range() const noexcept { return age_range_; }

std::string RuntimeContext::identifier() const noexcept { return scenario_->name(); }

void RuntimeContext::set_current_time(const int time_now) noexcept { time_now_ = time_now; }

void RuntimeContext::set_current_run(const unsigned int run_number) noexcept {
    current_run_ = run_number;
}

void RuntimeContext::reset_population(const std::size_t initial_pop_size) {
    population_ = Population{initial_pop_size};
    model_start_time_ = inputs_->start_time();
}

void RuntimeContext::publish(std::unique_ptr<EventMessage> message) const noexcept {
    // Modified- Mahima
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    // const_cast is needed here because we have a const method publishing to a non-const bus
    // This is safe because publish() doesn't modify the bus itself, only sends a message through it
    // Pointed out by Clang-Tidy
    const_cast<EventAggregator *>(event_bus_.get())
        ->publish(std::move(message)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

void RuntimeContext::publish_async(std::unique_ptr<EventMessage> message) const noexcept {
    // Modified- Mahima
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    // const_cast is needed here because we have a const method publishing to a non-const bus
    // This is safe because publish_async() doesn't modify the bus itself, only sends a message
    // through it Pointed out by Clang-Tidy
    const_cast<EventAggregator *>(event_bus_.get())
        ->publish_async(std::move(message)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

std::unordered_map<core::Region, double>
RuntimeContext::get_region_probabilities(int age, core::Gender gender) const {
    return inputs_->get_region_probabilities(age, gender);
}

std::unordered_map<core::Ethnicity, double>
RuntimeContext::get_ethnicity_probabilities(int age, core::Gender gender,
                                            core::Region region) const {
    return inputs_->get_ethnicity_probabilities(age, gender, region);
}

} // namespace hgps
