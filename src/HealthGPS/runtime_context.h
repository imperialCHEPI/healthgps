#pragma once

#include "event_aggregator.h"
#include "mapping.h"
#include "population.h"
#include "random_algorithm.h"
#include "runtime_metric.h"
#include "simulation_definition.h"

#include <functional>
#include <vector>

namespace hgps {

/// @brief Defines the Simulation runtime context data type
///
/// @details The context holds the simulation runtime information,
/// including the virtual population, and expose to all modules via
/// the API calls. Only one context instance exists per simulation
/// instance for internal use only by the engine algorithm.
class RuntimeContext {
  public:
    RuntimeContext() = delete;
    /// @brief Initialises a new instance of the RuntimeContext class.
    /// @param bus The message bus instance for communication with the outside world
    /// @param definition The simulation experiment definition.
    RuntimeContext(EventAggregator &bus, SimulationDefinition &definition);

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

    /// @brief Gets a reference to the simulation experiment scenario
    /// @return Experiment scenario
    Scenario &scenario() noexcept;

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
    std::string identifier() const noexcept;

    /// @brief Sets the current simulation time value
    /// @param time_now The new simulation time
    void set_current_time(const int time_now) noexcept;

    /// @brief Sets the current simulation run number
    /// @param run_number The new run number
    void set_current_run(const unsigned int run_number) noexcept;

    /// @brief Resets the virtual population to an initial size, with only new individuals.
    /// @param initial_pop_size Initial population size
    /// @param model_start_time Experiment start time
    void reset_population(const std::size_t initial_pop_size, const int model_start_time);

    /// @brief Publishes a polymorphic new message to the outside world synchronously.
    /// @param message The message instance to publish
    void publish(std::unique_ptr<EventMessage> message) const noexcept;

    /// @brief Publishes a polymorphic new message to the outside world asynchronously
    /// @param message The message instance to publish
    void publish_async(std::unique_ptr<EventMessage> message) const noexcept;

  private:
    std::reference_wrapper<EventAggregator> event_bus_;
    std::reference_wrapper<SimulationDefinition> definition_;
    Population population_;
    mutable Random generator_;
    RuntimeMetric metrics_{};
    unsigned int current_run_{};
    int model_start_time_{};
    int time_now_{};
};
} // namespace hgps
