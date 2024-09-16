#pragma once
#include "event_aggregator.h"
#include "simulation.h"

#include <atomic>
#include <functional>
#include <memory>
#include <stop_token>

namespace hgps {

/// @brief Defines the simulation executive data type class.
///
/// @details The simulation executive holds the simulation engine instances
/// and manages the execution of experiments by completing multiple runs
/// of the same scenario. The executive controls the number of runs required,
/// current run number, progress notifications, random number generator seeds
/// for each run, and the parallel evaluation for the two scenarios..
///
/// Health-GPS uses a simpler version of adevs (A Discrete EVent System simulator)
/// library (https://sourceforge.net/projects/bdevs), which contain only four header
/// files, but provides an intuitive modelling interface for agent-based models,
/// without requiring familiarity with the full aspects of the DEVS formalism.
class Runner {
  public:
    Runner() = delete;

    /// @brief Initialises a new instance of the Runner class.
    /// @param bus The message bus instance to use for notification
    /// @param seed_generator Master RNG for RNG seed generation
    Runner(std::shared_ptr<EventAggregator> bus,
           std::unique_ptr<RandomBitGenerator> seed_generator) noexcept;

    /// @brief Run an experiment for baseline scenario only
    /// @param baseline The simulation engine instance
    /// @param trial_runs Experiment number of runs
    /// @return The experiment total elapsed time, in milliseconds
    /// @throws std::invalid_argument for non-baseline scenario type or negative number of runs.
    double run(Simulation &baseline, const unsigned int trial_runs);

    /// @brief Run an experiment for baseline and intervention scenarios
    /// @param baseline The simulation engine instance for baseline scenario
    /// @param intervention The simulation engine instance for intervention scenario
    /// @param trial_runs Experiment number of runs
    /// @return The experiment total elapsed time, in milliseconds
    /// @throws std::invalid_argument for scenario type mismatch or negative number of runs.
    double run(Simulation &baseline, Simulation &intervention, const unsigned int trial_runs);

    /// @brief Gets a value indicating whether an experiment is current running.
    /// @return true, if a experiment is underway; otherwise, false.
    bool is_running() const noexcept;

    /// @brief Cancel a running experiment.
    void cancel() noexcept;

  private:
    std::atomic<bool> running_;
    std::shared_ptr<EventAggregator> event_bus_;
    std::unique_ptr<RandomBitGenerator> seed_generator_;
    std::stop_source source_;
    std::string runner_id_{};

    void run_model_thread(const std::stop_token &token, Simulation &model, unsigned int run,
                          unsigned int seed);

    void notify(std::unique_ptr<hgps::EventMessage> message);
};

} // namespace hgps
