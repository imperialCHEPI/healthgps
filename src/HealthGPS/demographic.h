#pragma once

#include "interfaces.h"
#include "repository.h"
#include "modelinput.h"
#include "runtime_context.h"
#include "gender_value.h"
#include "gender_table.h"
#include "life_table.h"

namespace hgps {

	struct PopulationRecord {
		PopulationRecord(int pop_age, float num_males, float num_females)
			: age{ pop_age }, males{ num_males }, females{ num_females } {}

		int age{};
		float males{};
		float females{};

		float total() const noexcept { return males + females; }
	};

	class PopulationModule final : public DemographicModule {
	public:
		PopulationModule() = delete;
		PopulationModule(std::map<int, std::map<int, PopulationRecord>>&& pop_data,
			LifeTable&& life_table);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		std::size_t get_total_population_size(const int time_year) const noexcept override;

		const std::map<int, PopulationRecord>& get_population_distribution(const int time_year) const override;

		void initialise_population(RuntimeContext& context) override;

		void update_population(RuntimeContext& context, const DiseaseHostModule& disease_host) override;

	private:
		std::map<int, std::map<int, PopulationRecord>> pop_data_;
		LifeTable life_table_;
		GenderTable<int, double> birth_rates_;
		GenderTable<int, double> residual_death_rates_;

		void initialise_birth_rates();

		double get_total_deaths(const int time_year) const noexcept;
		std::map<int, DoubleGenderValue> get_age_gender_distribution(const int time_year) const noexcept;
		DoubleGenderValue get_birth_rate(const int time_year) const noexcept;
		double get_residual_death_rate(const int age, const core::Gender gender) const noexcept;

		GenderTable<int, double> create_death_rates_table(const int time_year);
		GenderTable<int, double> calculate_residual_mortality(
			RuntimeContext& context, const DiseaseHostModule& disease_host);

		void update_residual_mortality(RuntimeContext& context, const DiseaseHostModule& disease_host);
		double calculate_excess_mortality_product(const Person& entity, const DiseaseHostModule& disease_host) const;
		int update_age_and_death_events(RuntimeContext& context, const DiseaseHostModule& disease_host);
	};

	std::unique_ptr<PopulationModule> build_population_module(Repository& repository, const ModelInput& config);
}

