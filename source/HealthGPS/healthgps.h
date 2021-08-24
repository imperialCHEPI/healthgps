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
		std::shared_ptr<DiseaseModule> disease_;
		std::shared_ptr<AnalysisModule> analysis_;
		adevs::Time end_time_;

		void initialise_population(const int pop_size, const int ref_year);
	};
}