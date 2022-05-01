#pragma once

#include "interfaces.h"
#include "repository.h"
#include "modelinput.h"
#include "runtime_context.h"
#include "analysis_definition.h"
#include "result_message.h"
#include "weight_model.h"

namespace hgps {

	class AnalysisModule final : public UpdatableModule {
	public:
		AnalysisModule() = delete;
		AnalysisModule(AnalysisDefinition&& definition, WeightModel&& classifier, const core::IntegerInterval age_range);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		void initialise_population(RuntimeContext& context) override;

		void update_population(RuntimeContext& context) override;

	private:
		AnalysisDefinition definition_;
		WeightModel weight_classifier_;
		DoubleAgeGenderTable residual_disability_weight_;

		double calculate_residual_disability_weight(const int& age, const core::Gender gender,
			const DoubleAgeGenderTable& expected_sum, const IntegerAgeGenderTable& expected_count);

		void publish_result_message(RuntimeContext& context) const;
		ModelResult calculate_historical_statistics(RuntimeContext& context) const;
		double calculate_disability_weight(const Person& entity) const;
		DALYsIndicator calculate_dalys(Population& population,
			const unsigned int& max_age, const unsigned int& death_year) const;
	};

	std::unique_ptr<AnalysisModule> build_analysis_module(Repository& repository, const ModelInput& config);
}