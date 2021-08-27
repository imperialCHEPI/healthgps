#include "diabetes_model.h"
#include "runtime_context.h"

namespace hgps {

	DiabetesModel::DiabetesModel(DiseaseDefinition&& definition, core::IntegerInterval age_range)
		: definition_{ definition }, average_relative_risk_{create_age_gender_table<double>(age_range)}
	{
		if (!core::case_insensitive::equals(definition.identifier().code, "diabetes")) {
			throw std::invalid_argument(
				std::format("Disease table data type mismatch: '{}' vs. 'diabetes'.",
					definition.identifier().code));
		}
	}

	std::string DiabetesModel::type() const noexcept { return definition_.identifier().code; }

	void DiabetesModel::initialise_disease_status(RuntimeContext& context) {
		auto relative_risk_table = calculate_average_relative_risk(context);
		auto prevalence_id = definition_.table().at("prevalence");
		for (auto& entity : context.population()) {
			if (!entity.is_active() || !definition_.table().contains(entity.age)) {
				continue;
			}

			auto relative_risk_value = calculate_relative_risk_for_risk_factors(entity);
			auto average_relative_risk = relative_risk_table(entity.age, entity.gender);
			auto prevalence = definition_.table()(entity.age, entity.gender).at(prevalence_id);
			auto probability = prevalence * relative_risk_value / average_relative_risk;
			auto hazard = context.next_double();
			if (hazard < probability) {
				entity.diseases[type()] = Disease{
					.status = DiseaseStatus::Active,
					.start_time = context.time_now() };
			}
		}
	}

	void DiabetesModel::initialise_average_relative_risk(RuntimeContext& context) {
		auto age_range = context.age_range();
		auto sum = create_age_gender_table<double>(age_range);
		auto count = create_age_gender_table<double>(age_range);
		for (auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			auto combine_risk = calculate_combined_relative_risk(entity, context.time_now());
			sum(entity.age, entity.gender) += combine_risk;
			count(entity.age, entity.gender)++;
		}

		for (int age = age_range.lower(); age <= age_range.upper(); age++) {
			if (sum.contains(age)) {
				auto male_count = count(age, core::Gender::male);
				if (male_count != 0.0) {
					average_relative_risk_(age, core::Gender::male) =
						sum(age, core::Gender::male) / male_count;
				}

				auto female_count = count(age, core::Gender::female);
				if (female_count != 0.0) {
					average_relative_risk_(age, core::Gender::female) =
						sum(age,core::Gender::female) / female_count;
				}
			}
		}
	}

	void DiabetesModel::update(RuntimeContext& context) {
		throw std::logic_error("DiabetesModel.update function not yet implemented.");
	}

	double DiabetesModel::get_excess_mortality(const int& age, const core::Gender& gender) const noexcept {
		auto excess_mortality_id = definition_.table().at("mtexcess");
		if (definition_.table().contains(age)) {
			return definition_.table()(age, gender).at(excess_mortality_id);
		}

		return 0.0;
	}

	DoubleAgeGenderTable DiabetesModel::calculate_average_relative_risk(RuntimeContext& context)
	{
		auto age_range = context.age_range();
		auto sum = create_age_gender_table<double>(age_range);
		auto count = create_age_gender_table<double>(age_range);
		for (auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			auto combine_risk = calculate_relative_risk_for_risk_factors(entity);
			sum(entity.age, entity.gender) += combine_risk;
			count(entity.age, entity.gender)++;
		}

		auto result = create_age_gender_table<double>(age_range);
		for (int age = age_range.lower(); age <= age_range.upper(); age++) {
			if (sum.contains(age)) {
				auto male_count = count(age, core::Gender::male);
				if (male_count != 0.0) {
					result(age, core::Gender::male) = sum(age, core::Gender::male) / male_count;
				}

				auto female_count = count(age, core::Gender::female);
				if (female_count != 0.0) {
					result(age, core::Gender::female) = sum(age, core::Gender::female) / female_count;
				}
			}
		}

		return result;
	}

	double DiabetesModel::calculate_combined_relative_risk(Person& entity, const int time_now) {
		auto combined_risk_value = 1.0;
		combined_risk_value *= calculate_relative_risk_for_risk_factors(entity);
		combined_risk_value *= calculate_relative_risk_for_diseases(entity, time_now);
		return combined_risk_value;
	}

	double DiabetesModel::calculate_relative_risk_for_risk_factors(Person& entity) {
		auto relative_risk_value = 1.0;
		for (auto& factor : entity.risk_factors) {
			if (!definition_.relative_risk_factors().contains(factor.first)) {
				continue;
			}

			auto lut = definition_.relative_risk_factors().at(factor.first).at(entity.gender);
			auto entity_factor_value = static_cast<float>(factor.second);
			auto relative_factor_value = lut(entity.age, entity_factor_value);
			relative_risk_value *= relative_factor_value;
		}

		return relative_risk_value;
	}

	double DiabetesModel::calculate_relative_risk_for_diseases(Person& entity, const int time_now) {
		auto relative_risk_value = 1.0;
		for (auto& disease : entity.diseases) {
			// Only include existing diseases
			if (time_now == 0 || disease.second.start_time < time_now) {
				double relative_disease_vale =
					definition_.relative_risk_diseases().at(disease.first)(entity.age, entity.gender);

				relative_risk_value *= relative_disease_vale;
			}
		}

		return relative_risk_value;
	}
}
