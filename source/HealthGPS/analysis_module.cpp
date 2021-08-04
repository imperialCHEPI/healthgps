
#include "analysis_module.h"
#include "converter.h"

namespace hgps {
	AnalysisModule::AnalysisModule(
		AnalysisDefinition&& definition, const core::IntegerInterval age_range)
		: definition_{ definition }, residual_disability_weight_{ create_age_gender_table<double>(age_range) }
	{}

	SimulationModuleType AnalysisModule::type() const noexcept {
		return SimulationModuleType::Analysis;
	}

	std::string AnalysisModule::name() const noexcept {
		return "Analysis";
	}

	void AnalysisModule::initialise_population(RuntimeContext& context) {
		auto age_range = context.age_range();
		auto expected_sum = create_age_gender_table<double>(age_range);
		auto expected_count = create_age_gender_table<int>(age_range);

		for (const auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			auto sum = 1.0;
			for (auto& disease : entity.diseases) {
				if (disease.second.status == DiseaseStatus::Active &&
					definition_.disability_weights().contains(disease.first)) {
					sum *= (1.0 - definition_.disability_weights().at(disease.first));
				}
			}

			expected_sum(entity.age, entity.gender) += sum;
			expected_count(entity.age, entity.gender)++;
		}

		for (int age = age_range.lower(); age <= age_range.upper(); age++) {
			residual_disability_weight_(age, core::Gender::male) = 
				calculate_residual_disability_weight(age, core::Gender::male, expected_sum, expected_count);

			residual_disability_weight_(age, core::Gender::female) = 
				calculate_residual_disability_weight(age, core::Gender::female, expected_sum, expected_count);
		}
	}

	double AnalysisModule::calculate_residual_disability_weight(const int& age, const core::Gender gender,
		const DoubleAgeGenderTable& expected_sum, const IntegerAgeGenderTable& expected_count)
	{
		auto residual_value = 0.0;
		if (!expected_sum.contains(age) || !definition_.observed_YLD().contains(age)) {
			return residual_value;
		}

		auto denominator = expected_count(age, gender);
		if (denominator != 0.0) {
			auto expected_mean = expected_sum(age, gender) / denominator;
			auto observed_YLD = definition_.observed_YLD()(age, gender);
			residual_value = 1.0 - (1.0 - observed_YLD) / expected_mean;
			if (std::isnan(residual_value)) {
				residual_value = 0.0;
			}
		}

		return residual_value;
	}

	std::unique_ptr<AnalysisModule> build_analysis_module(core::Datastore& manager, ModelInput& config)
	{
		auto analysis_entity = manager.get_disease_analysis(config.settings().country());
		if (analysis_entity.empty()) {
			throw std::logic_error(
				"Failed to create analysis module, invalid disease analysis definition.");
		}

		auto definition = detail::StoreConverter::to_analysis_definition(
			analysis_entity, config.settings().age_range());

		return std::make_unique<AnalysisModule>(std::move(definition), config.settings().age_range());
	}
}
