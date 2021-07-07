#pragma once

#include <vector>

#include "simulation.h"
#include "modulefactory.h"
#include "runtime_context.h"
#include "demographic.h"
#include "ses.h"
#include "riskfactor.h"

namespace hgps {

	class HealthGPS : public Simulation
	{
	public:
		HealthGPS() = delete;
		explicit HealthGPS(SimulationModuleFactory& factory, ModelInput& config, RandomBitGenerator&& generator);
		
		void initialize() override;

		void terminate() override;

		adevs::Time init(adevs::SimEnv<int>* env);

		adevs::Time update(adevs::SimEnv<int>* env);

		adevs::Time update(adevs::SimEnv<int>*, std::vector<int>&);

		void fini(adevs::Time clock);

	private:
		SimulationModuleFactory& factory_;
		RuntimeContext context_;
		std::shared_ptr<SESModule> ses_;
		std::shared_ptr<DemographicModule> demographic_;
		std::shared_ptr<RiskFactorModule> risk_factor_;

		void initialise_population(const int pop_size, const int ref_year);
	};
}