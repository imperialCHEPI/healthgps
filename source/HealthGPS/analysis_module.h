#pragma once

#include "interfaces.h"
#include "modelinput.h"
#include "runtime_context.h"
#include "analysis_definition.h"

namespace hgps {

	class AnalysisModule final : public SimulationModule {
	public:
		AnalysisModule() = delete;
		AnalysisModule(AnalysisDefinition&& definition, const core::IntegerInterval age_range);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		void initialise_population(RuntimeContext& context);

	private:
		AnalysisDefinition definition_;
		DoubleAgeGenderTable residual_disability_weight_;

		double calculate_residual_disability_weight(const int& age, const core::Gender gender,
			const DoubleAgeGenderTable& expected_sum, const IntegerAgeGenderTable& expected_count);
	};

	std::unique_ptr<AnalysisModule> build_analysis_module(core::Datastore& manager, ModelInput& config);
}