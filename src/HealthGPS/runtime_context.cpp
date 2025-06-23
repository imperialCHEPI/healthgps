#include "runtime_context.h"
#include "HealthGPS.Core/exception.h"
#include "risk_factor_inspector.h"

namespace hgps {

// MAHIMA: TOGGLE FOR YEAR 3 RISK FACTOR INSPECTION
// Must match the toggle in static_linear_model.cpp and simulation.cpp
static constexpr bool ENABLE_YEAR3_RISK_FACTOR_INSPECTION = false;

RuntimeContext::RuntimeContext(std::shared_ptr<const EventAggregator> bus,
                               std::shared_ptr<const ModelInput> inputs,
                               std::unique_ptr<Scenario> scenario)
    : event_bus_{std::move(bus)}, inputs_{std::move(inputs)}, scenario_{std::move(scenario)},
      population_{0} {}

// MAHIMA: Destructor implementation for RuntimeContext
// This needs to be defined in the .cpp file where RiskFactorInspector complete type is available
// This resolves the "incomplete type" error with std::unique_ptr<RiskFactorInspector>
RuntimeContext::~RuntimeContext() = default;

int RuntimeContext::time_now() const noexcept { return time_now_; }

int RuntimeContext::start_time() const noexcept { return model_start_time_; }

unsigned int RuntimeContext::current_run() const noexcept { return current_run_; }

int RuntimeContext::sync_timeout_millis() const noexcept {
    int timeout = inputs_->sync_timeout_ms();
    // std::cout << "\nDEBUG: RuntimeContext::sync_timeout_millis - Value: " << timeout << "ms";
    return timeout;
}

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

const core::IntegerInterval &RuntimeContext::age_range() const noexcept {
    return inputs_->settings().age_range();
}

const std::string &RuntimeContext::identifier() const noexcept { return scenario_->name(); }

void RuntimeContext::set_current_time(const int time_now) noexcept { time_now_ = time_now; }

void RuntimeContext::set_current_run(const unsigned int run_number) noexcept {
    current_run_ = run_number;
}

void RuntimeContext::reset_population(const std::size_t initial_pop_size) {
    population_ = Population{initial_pop_size};
    model_start_time_ = inputs_->start_time();
}

void RuntimeContext::publish(std::unique_ptr<EventMessage> message) const noexcept {
    event_bus_->publish(std::move(message));
}

void RuntimeContext::publish_async(std::unique_ptr<EventMessage> message) const noexcept {
    event_bus_->publish_async(std::move(message));
}

double RuntimeContext::ensure_risk_factor_in_range(const core::Identifier &factor_key,
                                                   double value) const noexcept {
    try {
        // Look up the MappingEntry for this risk factor
        const MappingEntry &entry = mapping().at(factor_key);

        // Use the MappingEntry's get_bounded_value method to clamp the value to its range
        return entry.get_bounded_value(value);
    } catch (const std::exception &) {
        // If the factor is not found or any other error occurs, return the original value
        return value;
    }
}

void RuntimeContext::set_risk_factor_inspector(std::unique_ptr<RiskFactorInspector> inspector) {
    risk_factor_inspector_ = std::move(inspector);

    if constexpr (ENABLE_YEAR3_RISK_FACTOR_INSPECTION) {
        if (risk_factor_inspector_) {
            std::cout << "\nMAHIMA: Risk Factor Inspector successfully set in RuntimeContext";
            std::cout << "\n  Scenario: " << scenario_->name();
            std::cout << "\n  Ready for Year 3 data capture.\n";
        } else {
            std::cout << "\nMAHIMA: Warning - Null Risk Factor Inspector set in RuntimeContext\n";
        }
    }
}

bool RuntimeContext::has_risk_factor_inspector() const noexcept {
    bool has_inspector = (risk_factor_inspector_ != nullptr);

    return has_inspector;
}

RiskFactorInspector &RuntimeContext::get_risk_factor_inspector() {
    if (!risk_factor_inspector_) {
        throw core::HgpsException(
            "MAHIMA: RuntimeContext::get_risk_factor_inspector() called but no inspector is set! "
            "Make sure to call has_risk_factor_inspector() first and set_risk_factor_inspector() "
            "during simulation initialization.");
    }

    return *risk_factor_inspector_;
}

} // namespace hgps
