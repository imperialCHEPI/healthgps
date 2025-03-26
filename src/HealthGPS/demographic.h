#pragma once

#include "disease.h"
#include "gender_table.h"
#include "gender_value.h"
#include "interfaces.h"
#include "life_table.h"
#include "modelinput.h"
#include "repository.h"
#include "runtime_context.h"
#include "static_linear_model.h"

namespace hgps {

/// @brief Implements the population demographic module data type
class DemographicModule final : public SimulationModule {
  public:
    DemographicModule() = delete;

    /// @brief Initialise a new instance of the DemographicModule class.
    /// @param pop_data Population demographic trends table with year and age lookup
    /// @param life_table Population life trends table with births and deaths
    /// @param income_models Income models for different income levels
    /// @param region_models Region models for different regions
    /// @param ethnicity_models Ethnicity models for different ethnicities
    /// @param income_continuous_stddev Standard deviation for continuous income
    /// @param physical_activity_stddev Standard deviation for physical activity
    DemographicModule(std::map<int, std::map<int, PopulationRecord>> &&pop_data,
                      LifeTable &&life_table,
                      std::unordered_map<core::Income, LinearModelParams> &&income_models,
                      std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> &&region_models,
                      std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> &&ethnicity_models,
                      double income_continuous_stddev,
                      double physical_activity_stddev);

    /// @brief Gets the module type identifier
    /// @return The module type identifier
    SimulationModuleType type() const noexcept override;

    /// @brief Gets the module name
    /// @return The human-readable module name
    std::string name() const noexcept override;

    /// @brief Gets the total population at a specific point in time
    /// @param time_year The reference point in time (in year)
    /// @return The respective total population size
    std::size_t get_total_population_size(int time_year) const noexcept;

    /// @brief Gets the population age distribution at a specific point in time
    /// @param time_year The reference point in time (in year)
    /// @return The respective population age distribution
    const std::map<int, PopulationRecord> &get_population_distribution(int time_year) const;

    /// @brief Initialises the virtual population status
    /// @param context The simulation run-time context
    void initialise_population(RuntimeContext &context, Population &population, Random &random);

    /// @brief Updates the virtual population status
    /// @param context The simulation run-time context
    void update_population(RuntimeContext &context) override;

    /// @brief Updates the virtual population status
    /// @param context The simulation run-time context
    /// @param disease_host The diseases host module instance
    void update_population(RuntimeContext &context, const DiseaseModule &disease_host);

    void initialise_region(RuntimeContext &context, Person &person, Random &random);
    void initialise_ethnicity(RuntimeContext &context, Person &person, Random &random);
    void initialise_income_continuous(Person &person, Random &random);
    void initialise_income_category(Person &person, double q1_threshold, double q2_threshold, double q3_threshold);
    void initialise_physical_activity(RuntimeContext &context, Person &person, Random &random);
    std::tuple<double, double, double> calculate_income_thresholds(const Population &population);

  private:
    std::map<int, std::map<int, PopulationRecord>> pop_data_;
    LifeTable life_table_;
    GenderTable<int, double> birth_rates_;
    GenderTable<int, double> residual_death_rates_;
    std::string name_{"Demographic"};

    std::unordered_map<core::Income, LinearModelParams> income_models_;
    std::shared_ptr<std::unordered_map<core::Region, LinearModelParams>> region_models_;
    std::shared_ptr<std::unordered_map<core::Ethnicity, LinearModelParams>> ethnicity_models_;
    double income_continuous_stddev_;
    double physical_activity_stddev_;

    // Risk factor tables
    std::shared_ptr<UnorderedMap2d<core::Gender, core::Identifier, std::map<int, double>>> expected_;
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_;
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps_;

    void initialise_birth_rates();
    void initialise_age_gender(RuntimeContext &context, Population &population, Random &random);
    double get_expected(RuntimeContext &context, core::Gender gender, int age,
                       const core::Identifier &factor, std::optional<core::DoubleInterval> range,
                       bool apply_trend) const noexcept;

    double get_total_deaths(int time_year) const noexcept;
    std::map<int, DoubleGenderValue> get_age_gender_distribution(int time_year) const noexcept;
    DoubleGenderValue get_birth_rate(int time_year) const noexcept;
    double get_residual_death_rate(int age, core::Gender gender) const noexcept;

    GenderTable<int, double> create_death_rates_table(int time_year);
    GenderTable<int, double> calculate_residual_mortality(RuntimeContext &context,
                                                          const DiseaseModule &disease_host);

    void update_residual_mortality(RuntimeContext &context, const DiseaseModule &disease_host);
    static double calculate_excess_mortality_product(const Person &entity,
                                                     const DiseaseModule &disease_host);
    int update_age_and_death_events(RuntimeContext &context, const DiseaseModule &disease_host);
    int get_trend_steps(const core::Identifier &factor) const noexcept;
};

/// @brief Builds a new instance of the DemographicModule using the given data infrastructure
/// @param repository The data repository instance
/// @param config The model inputs instance
/// @return A new DemographicModule instance
std::unique_ptr<DemographicModule> build_population_module(Repository &repository,
                                                           const ModelInput &config);

} // namespace hgps
