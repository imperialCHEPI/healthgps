#pragma once

#include "gender_table.h"
#include "gender_value.h"
#include "interfaces.h"
#include "life_table.h"
#include "modelinput.h"
#include "repository.h"
#include "runtime_context.h"

namespace hgps {

/// @brief Implements the population demographic module data type
class PopulationModule final : public DemographicModule {
  public:
    PopulationModule() = delete;

    /// @brief Initialise a new instance of the PopulationModule class.
    /// @param pop_data Population demographic trends table with year and age lookup
    /// @param life_table Population life trends table with births and deaths
    PopulationModule(std::map<int, std::map<int, PopulationRecord>> &&pop_data,
                     LifeTable &&life_table);

    SimulationModuleType type() const noexcept override;

    const std::string &name() const noexcept override;

    std::size_t get_total_population_size(int time_year) const noexcept override;

    const std::map<int, PopulationRecord> &
    get_population_distribution(int time_year) const override;

    void initialise_population(RuntimeContext &context) override;

    void update_population(RuntimeContext &context, const DiseaseHostModule &disease_host) override;

  private:
    std::map<int, std::map<int, PopulationRecord>> pop_data_;
    LifeTable life_table_;
    GenderTable<int, double> birth_rates_;
    GenderTable<int, double> residual_death_rates_;
    std::string name_{"Demographic"};

    void initialise_birth_rates();

    double get_total_deaths(int time_year) const noexcept;
    std::map<int, DoubleGenderValue> get_age_gender_distribution(int time_year) const noexcept;
    DoubleGenderValue get_birth_rate(int time_year) const noexcept;
    double get_residual_death_rate(int age, core::Gender gender) const noexcept;

    GenderTable<int, double> create_death_rates_table(int time_year);
    GenderTable<int, double> calculate_residual_mortality(RuntimeContext &context,
                                                          const DiseaseHostModule &disease_host);

    void update_residual_mortality(RuntimeContext &context, const DiseaseHostModule &disease_host);
    static double calculate_excess_mortality_product(const Person &entity,
                                                     const DiseaseHostModule &disease_host);
    int update_age_and_death_events(RuntimeContext &context, const DiseaseHostModule &disease_host);
};

/// @brief Builds a new instance of the PopulationModule using the given data infrastructure
/// @param repository The data repository instance
/// @param config The model inputs instance
/// @return A new PopulationModule instance
std::unique_ptr<PopulationModule> build_population_module(Repository &repository,
                                                          const ModelInput &config);
} // namespace hgps
