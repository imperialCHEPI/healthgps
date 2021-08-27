#pragma once

#include "interfaces.h"
#include "modelinput.h"
#include "runtime_context.h"
#include "gender_table.h"
#include "life_table.h"

namespace hgps {

	struct PopulationRecord {
		PopulationRecord(int pop_age, float num_males, float num_females)
			: age{ pop_age }, males{ num_males }, females{ num_females }
		{}

		const int age{};
		const float males{};
		const float females{};

		const float total() const noexcept { return males + females; }
	};

	struct GenderPair {
		GenderPair(double male_value, double female_value) 
			: male{ male_value }, female{ female_value }
		{}

		const double male;

		const double female;
	};

	class DemographicModule final : public SimulationModule {
	public:
		DemographicModule() = delete;
		DemographicModule(std::map<int, std::map<int, PopulationRecord>>&& pop_data,
			LifeTable&& life_table);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		size_t get_total_population(const int time_year) const noexcept;

		std::map<int, GenderPair> get_age_gender_distribution(const int time_year) const noexcept;

		GenderPair get_birth_rate(const int time_year) const noexcept;

		void initialise_population(RuntimeContext& context) override;

		void update_residual_mortality(RuntimeContext& context, const DiseaseHostModule& disease_host);

	private:
		std::map<int, std::map<int, PopulationRecord>> pop_data_;
		LifeTable life_table_;
		GenderTable<int, double> birth_rates_;
		GenderTable<int, double> residual_death_rates_;

		void initialise_birth_rates();

		GenderTable<int, double> create_death_rates_table(const int time_year);
		double calculate_excess_mortality_product(const Person& entity, const DiseaseHostModule& disease_host) const;
	};

	std::unique_ptr<DemographicModule> build_demographic_module(core::Datastore& manager, ModelInput& config);
}

