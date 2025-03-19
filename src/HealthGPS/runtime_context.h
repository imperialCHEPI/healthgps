#pragma once

#include "event_aggregator.h"
#include "mapping.h"
#include "modelinput.h"
#include "population.h"
#include "random_algorithm.h"
#include "runtime_metric.h"
#include "scenario.h"

#include <functional>
#include <map>
#include <tuple>
#include <unordered_map>
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
    /// @param inputs The simulation model inputs
    /// @param scenario The scenario to simulate
    RuntimeContext(std::shared_ptr<const EventAggregator> bus,
                   std::shared_ptr<const ModelInput> inputs, std::unique_ptr<Scenario> scenario);

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
    std::string identifier() const noexcept;

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

    /// @brief Get region probabilities for specific age and gender stratum
    /// @param age The age stratum
    /// @param gender The gender stratum
    /// @return Map of region to probability for this stratum
    std::unordered_map<core::Region, double> get_region_probabilities(int age,
                                                                      core::Gender gender) const;

    /// @brief Get ethnicity probabilities for specific age, gender, and region stratum
    /// @param age The age stratum
    /// @param gender The gender stratum
    /// @param region The region stratum
    /// @return Map of ethnicity to probability for this stratum
    std::unordered_map<core::Ethnicity, double>
    get_ethnicity_probabilities(int age, core::Gender gender, core::Region region) const;

  private:
    mutable std::shared_ptr<EventAggregator>
        event_bus_; // NOLINT(cppcoreguidelines-pro-type-const-cast)
    std::shared_ptr<const ModelInput> inputs_;
    std::unique_ptr<Scenario> scenario_;
    Population population_;
    mutable Random random_{};
    RuntimeMetric metrics_;
    int time_now_{};
    unsigned int current_run_{};
    int model_start_time_{};
    core::IntegerInterval age_range_;
    mutable std::string cached_name_;
};

} // namespace hgps
