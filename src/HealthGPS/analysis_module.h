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
		std::vector<std::string> channels_;

		double calculate_residual_disability_weight(const int& age, const core::Gender gender,
			const DoubleAgeGenderTable& expected_sum, const IntegerAgeGenderTable& expected_count);

		void publish_result_message(RuntimeContext& context) const;
		void calculate_historical_statistics(RuntimeContext& context, ModelResult& result) const;
		double calculate_disability_weight(const Person& entity) const;
		DALYsIndicator calculate_dalys(Population& population, unsigned int max_age, unsigned int death_year) const;

		void calculate_population_statistics(RuntimeContext& context, DataSeries& series) const;
		void classify_weight(hgps::DataSeries& series, const hgps::Person& entity) const;
		void initialise_output_channels(RuntimeContext& context);
	};

	std::unique_ptr<AnalysisModule> build_analysis_module(Repository& repository, const ModelInput& config);
}