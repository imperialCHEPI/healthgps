#pragma once

#include <vector>

#include "simulation.h"
#include "runtime_context.h"
#include "simulation_module.h"
#include "event_aggregator.h"

namespace hgps {

	class HealthGPS : public Simulation
	{
	public:
		HealthGPS() = delete;
		explicit HealthGPS(SimulationDefinition&& definition, SimulationModuleFactory& factory, EventAggregator& bus);
		
		void initialize() override;
		void terminate() override;

		adevs::Time init(adevs::SimEnv<int>* env);
		adevs::Time update(adevs::SimEnv<int>* env);
		adevs::Time update(adevs::SimEnv<int>*, std::vector<int>&);
		void fini(adevs::Time clock);

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
		hgps::IntegerAgeGenderTable	get_current_simulated_population();
		const std::vector<Person> get_similar_entities(const unsigned int& age, const core::Gender& gender);
		void apply_net_migration(const int net_value, const unsigned int& age, const core::Gender& gender);
		hgps::IntegerAgeGenderTable	get_net_migration();
		hgps::IntegerAgeGenderTable	create_net_migration();

		Person partial_clone_entity(const Person& source) const noexcept;
	};
}