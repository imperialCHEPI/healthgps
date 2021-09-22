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
		std::string name() override;

		adevs::Time init(adevs::SimEnv<int>* env);
		adevs::Time update(adevs::SimEnv<int>* env);
		adevs::Time update(adevs::SimEnv<int>*, std::vector<int>&);
		void fini(adevs::Time clock);

		void set_current_run(const unsigned int run_number) noexcept override;

	private:
		RuntimeContext context_;
		std::shared_ptr<SESModule> ses_;
		std::shared_ptr<DemographicModule> demographic_;
		std::shared_ptr<RiskFactorModule> risk_factor_;
		std::shared_ptr<DiseaseHostModule> disease_;
		std::shared_ptr<AnalysisModule> analysis_;
		adevs::Time end_time_;

		void initialise_population();
		void update_population();
		void print_initial_population_statistics();

		void update_age_and_lifecycle_events();
		int update_age_and_death_events();
		void update_net_immigration();

		hgps::IntegerAgeGenderTable get_current_expected_population() const;
		hgps::IntegerAgeGenderTable	get_current_simulated_population();
		std::vector<std::reference_wrapper<const Person>> get_similar_entities(const int& age, const core::Gender& gender);
		void apply_net_migration(int net_value, int& age, const core::Gender& gender);

		Person partial_clone_entity(const Person& source) const noexcept;
	};
}