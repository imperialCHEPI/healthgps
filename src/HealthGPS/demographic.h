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
    /// @param region_prevalence Region prevalence by age group and gender
    /// @param ethnicity_prevalence Ethnicity prevalence by age group and gender
    /// @param income_models Income models for different income classifications
    DemographicModule(std::map<int, std::map<int, PopulationRecord>> &&pop_data,
                      LifeTable &&life_table,
                      std::unordered_map<core::Identifier, std::unordered_map<core::Gender, std::unordered_map<core::Region, double>>> region_prevalence,
                      std::unordered_map<core::Identifier, std::unordered_map<core::Gender, std::unordered_map<core::Ethnicity, double>>> ethnicity_prevalence,
                      std::unordered_map<core::Income, LinearModelParams> income_models);

    /// @brief Gets the module type identifier
    /// @return The module type identifier
    SimulationModuleType type() const noexcept override;

    /// @brief Gets the module name
    /// @return The human-readable module name
    const std::string &name() const noexcept override;

    /// @brief Gets the total population at a specific point in time
    /// @param time_year The reference point in time (in year)
    /// @return The respective total population size
    std::size_t get_total_population_size(int time_year) const noexcept;

    /// @brief Gets the population age distribution at a specific point in time
    /// @param time_year The reference point in time (in year)
    /// @return The respective population age distribution
    const std::map<int, PopulationRecord> &get_population_distribution(int time_year) const;

    /// @brief Initialises the virtual population with age and gender
    /// @param context The simulation run-time context
    void initialise_age_gender(RuntimeContext &context);

    /// @brief Initialises the virtual population status with all the varaibles of age, gender, region, ethnicity, income and physical activity
    /// @param context The simulation run-time context
    void initialise_population(RuntimeContext &context) override;

    /// @brief Updates the virtual population status
    /// @param context The simulation run-time context
    /// @param disease_host The diseases host module instance
    void update_population(RuntimeContext &context, const DiseaseModule &disease_host);

  private:
    std::map<int, std::map<int, PopulationRecord>> pop_data_;
    LifeTable life_table_;
    GenderTable<int, double> birth_rates_;
    GenderTable<int, double> residual_death_rates_;
    std::string name_{"Demographic"};
    std::vector<double> income_quartile_thresholds_;
    
    // Demographic configuration data from JSON
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, std::unordered_map<core::Region, double>>> region_prevalence_;
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, std::unordered_map<core::Ethnicity, double>>> ethnicity_prevalence_;
    std::unordered_map<core::Income, LinearModelParams> income_models_;

    void initialise_birth_rates();

    // Population-level initialization functions
    void initialise_region(RuntimeContext &context, Person &person, Random &random);
    void initialise_ethnicity(RuntimeContext &context, Person &person, Random &random);
    void initialise_income_continuous(RuntimeContext &context, Person &person, Random &random);
    void initialise_income_category(Person &person, const Population &population);
    void calculate_income_quartiles(const Population &population);
    void update_income_category(RuntimeContext &context);
    void initialize_newborns(RuntimeContext &context);

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
};

/// @brief Builds a new instance of the DemographicModule using the given data infrastructure
/// @param repository The data repository instance
/// @param config The model inputs instance
/// @return A new DemographicModule instance
std::unique_ptr<DemographicModule> build_population_module(Repository &repository,
                                                           const ModelInput &config);

} // namespace hgps
