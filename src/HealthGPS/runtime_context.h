#pragma once

#include "event_aggregator.h"
#include "mapping.h"
#include "modelinput.h"
#include "population.h"
#include "random_algorithm.h"
#include "runtime_metric.h"
#include "scenario.h"

#include <functional>
#include <vector>
#include <memory>

namespace hgps {

// MAHIMA: Forward declaration for RiskFactorInspector
// This prevents circular dependencies while allowing the context to manage the inspector
class RiskFactorInspector;

/// @brief Defines the Simulation runtime context data type
///
/// @details The context holds the simulation runtime information,
/// including the virtual population, and expose to all modules via
/// the API calls. Only one context instance exists per simulation
/// instance for internal use only by the engine algorithm.
/// 
/// MAHIMA: Extended to support Risk Factor Inspector for Year 3 policy inspection
class RuntimeContext {
  public:
    RuntimeContext() = delete;
    /// @brief Initialises a new instance of the RuntimeContext class.
    /// @param bus The message bus instance for communication with the outside world
    /// @param inputs The simulation model inputs
    /// @param scenario The scenario to simulate
    RuntimeContext(std::shared_ptr<const EventAggregator> bus,
                   std::shared_ptr<const ModelInput> inputs, std::unique_ptr<Scenario> scenario);

    /// @brief MAHIMA: Destructor for RuntimeContext
    /// @details Explicitly declared to handle std::unique_ptr<RiskFactorInspector> with forward declaration
    ~RuntimeContext();

    /// @brief Gets the current simulation time
    /// @return Current simulation time
    int time_now() const noexcept;

    /// @brief Gets the experiment start time
    /// @return Experiment start time
    int start_time() const noexcept;

    /// @brief Gets the experiment current run number
    /// @return Current run number
    unsigned int current_run() const noexcept;

    /// @brief Gets the data synchronisation timeout in milliseconds
    /// @return Timeout in milliseconds
    int sync_timeout_millis() const noexcept;

    /// @brief Gets a reference to the virtual population container
    /// @return Virtual population
    Population &population() noexcept;

    /// @brief Gets a reference to the virtual population container
    /// @return Virtual population
    const Population &population() const noexcept;

    /// @brief Gets a reference to the runtime metrics container
    /// @return Runtime metrics
    RuntimeMetric &metrics() noexcept;

    /// @brief Gets a reference to the model inputs definition
    /// @return The model inputs reference
    const ModelInput &inputs() const noexcept;

    /// @brief Gets a reference to the simulation scenario instance
    /// @return Experiment Scenario reference
    Scenario &scenario() const noexcept;

    /// @brief Gets a reference to the engine random number generator
    /// @return Random number generator
    Random &random() const noexcept;

    /// @brief Gets a read-only reference to the hierarchical risk factors mapping
    /// @return Risk factor mapping
    const HierarchicalMapping &mapping() const noexcept;

    /// @brief Gets a read-only reference to the select diseases information
    /// @return Selected diseases
    const std::vector<core::DiseaseInfo> &diseases() const noexcept;

    /// @brief Gets a read-only reference to the population age range constraints
    /// @return Allowed age range
    const core::IntegerInterval &age_range() const noexcept;

    /// @brief Gets the simulation identifier for outside world messages
    /// @return Simulation identifier
    const std::string &identifier() const noexcept;

    /// @brief Sets the current simulation time value
    /// @param time_now The new simulation time
    void set_current_time(const int time_now) noexcept;

    /// @brief Sets the current simulation run number
    /// @param run_number The new run number
    void set_current_run(const unsigned int run_number) noexcept;

    /// @brief Resets the virtual population to an initial size, with only new individuals.
    /// @param initial_pop_size Initial population size
    void reset_population(std::size_t initial_pop_size);

    /// @brief Publishes a polymorphic new message to the outside world synchronously.
    /// @param message The message instance to publish
    void publish(std::unique_ptr<EventMessage> message) const noexcept;

    /// @brief Publishes a polymorphic new message to the outside world asynchronously
    /// @param message The message instance to publish
    void publish_async(std::unique_ptr<EventMessage> message) const noexcept;

    /// @brief Ensures a risk factor value stays within its specified range in the configuration
    /// @param factor_key The risk factor identifier
    /// @param value The value to be clamped
    /// @return The value clamped to the factor's range if one exists, or the original value
    double ensure_risk_factor_in_range(const core::Identifier &factor_key,
                                       double value) const noexcept;

    // MAHIMA: Risk Factor Inspector methods for Year 3 policy inspection
    // These methods allow the simulation to manage a risk factor inspector instance
    // for capturing individual person data when policies are applied in Year 3

    /// @brief MAHIMA: Set the risk factor inspector instance
    /// @param inspector Unique pointer to the risk factor inspector
    /// 
    /// @details The inspector is used to capture individual person risk factor data
    /// in Year 3 when policies are applied. This helps identify outliers and debug
    /// any weird/incorrect values that appear after policy application.
    void set_risk_factor_inspector(std::unique_ptr<RiskFactorInspector> inspector);

    /// @brief MAHIMA: Check if a risk factor inspector is available
    /// @return true if inspector is set, false otherwise
    /// 
    /// @details This allows risk factor models to check if they should trigger
    /// data capture without causing null pointer exceptions.
    bool has_risk_factor_inspector() const noexcept;

    /// @brief MAHIMA: Get reference to the risk factor inspector
    /// @return Reference to the risk factor inspector
    /// @throws std::runtime_error if no inspector is set
    /// 
    /// @details This provides access to the inspector for triggering Year 3 data capture.
    /// Always check has_risk_factor_inspector() first to avoid exceptions.
    RiskFactorInspector& get_risk_factor_inspector();

    int NumberOfResultsCSVs = 4;

  private:
    std::shared_ptr<const EventAggregator> event_bus_;
    std::shared_ptr<const ModelInput> inputs_;
    std::unique_ptr<Scenario> scenario_;
    Population population_;
    mutable Random random_{};
    RuntimeMetric metrics_{};
    unsigned int current_run_{};
    int model_start_time_{};
    int time_now_{};

    // MAHIMA: Risk Factor Inspector for Year 3 policy analysis
    // This inspector captures individual person risk factor values when policies
    // are applied to help debug weird/incorrect values and identify outliers
    std::unique_ptr<RiskFactorInspector> risk_factor_inspector_;
};

} // namespace hgps
