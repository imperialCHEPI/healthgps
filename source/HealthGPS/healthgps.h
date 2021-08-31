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
		explicit HealthGPS(SimulationModuleFactory& factory, ModelInput& config,
						   EventAggregator& bus, RandomBitGenerator&& generator);
		
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

		void initialise_population(const int pop_size, const int ref_year);
		void update_population(const int initial_pop_size);
		void update_age_and_lifecycle_events();
		int update_age_and_death_events();

		void update_socioeconomic_status();
		void update_education_level(Person& entity,
			std::vector<int>& education_levels, std::vector<float>& education_freq);
		void update_income_level(Person& entity,
			std::vector<int>& income_levels, std::vector<float>& income_freq);

		void update_risk_factors();
	};
}