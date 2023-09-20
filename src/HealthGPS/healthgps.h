#pragma once

#include <vector>

#include "event_aggregator.h"
#include "runtime_context.h"
#include "simulation.h"
#include "simulation_module.h"

namespace hgps {

/// @brief Defines the simulation engine data type class.
///
/// @details The simulation engine holds the modules instances and run-time
/// context, manages the simulation clock and core algorithm sequencing.
///
/// Health-GPS uses a simpler version of adevs (A Discrete EVent System simulator)
/// library (https://sourceforge.net/projects/bdevs), which contain only four header
/// files, but provides an intuitive modelling interface for agent-based models,
/// without requiring familiarity with the full aspects of the DEVS formalism.
class HealthGPS : public Simulation {
  public:
    HealthGPS() = delete;

    /// @brief Initialises a new instance of the HealthGPS class
    /// @param definition The simulation definition instance
    /// @param factory The simulation modules factory instance
    /// @param bus The message bus instance to use
    explicit HealthGPS(SimulationDefinition &&definition, SimulationModuleFactory &factory,
                       EventAggregator &bus);

    void initialize() override;
    void terminate() override;

    /// @brief Called when the model is added to the simulation executive
    /// @param env The simulation executive environment
    /// @return The time of the next event
    adevs::Time init(adevs::SimEnv<int> *env) override;

    /// @brief Called to assign a new state to the model at current time
    /// @param env The simulation executive environment
    /// @return The time of the next call to update.
    adevs::Time update(adevs::SimEnv<int> *env) override;

    /// @brief Called to assign a new state to the model at current time, when input is present.
    ///
    /// @details This is not used with Health-GPS, the individuals are independent and nobody
    /// sends messages. If needed by future scenarios, e.g., agent-based models, the input was
    /// generated in the previous simulation instant and so this calculates the new state
    /// using state information from the previous instant.
    ///
    /// @param env The simulation executive environment
    /// @param x Messages sent to the model.
    /// @return The time of the next call to update.
    adevs::Time update(adevs::SimEnv<int> *env, std::vector<int> &x) override;

    /// @brief Called after the model has been removed from the simulation executive.
    /// @param clock The time at which the model no longer exists.
    void fini(adevs::Time clock) override;

    void setup_run(unsigned int run_number) noexcept override;

    void setup_run(unsigned int run_number, unsigned int seed) noexcept override;

  private:
    RuntimeContext context_;
    std::shared_ptr<UpdatableModule> ses_;
    std::shared_ptr<DemographicModule> demographic_;
    std::shared_ptr<RiskFactorHostModule> risk_factor_;
    std::shared_ptr<DiseaseHostModule> disease_;
    std::shared_ptr<UpdatableModule> analysis_;
    adevs::Time end_time_;

    void initialise_population();
    void update_population();
    void print_initial_population_statistics();

    void update_net_immigration();

    hgps::IntegerAgeGenderTable get_current_expected_population() const;
    hgps::IntegerAgeGenderTable get_current_simulated_population();
    void apply_net_migration(int net_value, unsigned int age, const core::Gender &gender);
    hgps::IntegerAgeGenderTable get_net_migration();
    hgps::IntegerAgeGenderTable create_net_migration();
    std::map<std::string, core::UnivariateSummary> create_input_data_summary() const;

    static Person partial_clone_entity(const Person &source) noexcept;
};
} // namespace hgps
