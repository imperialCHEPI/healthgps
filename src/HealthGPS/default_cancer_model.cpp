#include "default_cancer_model.h"
#include "runtime_context.h"
#include "person.h"

namespace hgps {

	DefaultCancerModel::DefaultCancerModel(DiseaseDefinition& definition, const core::IntegerInterval age_range)
		: definition_{ definition }, average_relative_risk_{ create_age_gender_table<double>(age_range) }
	{
		if (definition_.get().identifier().group != core::DiseaseGroup::cancer) {
			throw std::invalid_argument("Disease definition group mismatch, must be 'cancer'.");
		}
	}

	core::DiseaseGroup DefaultCancerModel::group() const noexcept {
		return definition_.get().identifier().group;
	}

	std::string DefaultCancerModel::disease_type() const noexcept {
		return definition_.get().identifier().code;
	}

	void DefaultCancerModel::initialise_disease_status(RuntimeContext& context) {
		auto relative_risk_table = calculate_average_relative_risk(context);
		auto prevalence_id = definition_.get().table().at("prevalence");
		for (auto& entity : context.population()) {
			if (!entity.is_active() || !definition_.get().table().contains(entity.age)) {
				continue;
			}

			auto relative_risk_value = calculate_relative_risk_for_risk_factors(entity);
			auto average_relative_risk = relative_risk_table(entity.age, entity.gender);
			auto prevalence = definition_.get().table()(entity.age, entity.gender).at(prevalence_id);
			auto probability = prevalence * relative_risk_value / average_relative_risk;
			auto hazard = context.random().next_double();
			if (hazard < probability) {
				auto onset = calculate_time_since_onset(context, entity.gender);
				entity.diseases[disease_type()] = Disease{
					.status = DiseaseStatus::active,
					.start_time = context.time_now(),
					.time_since_onset = onset };
			}
		}
	}

	void DefaultCancerModel::initialise_average_relative_risk(RuntimeContext& context) {
		auto age_range = context.age_range();
		auto sum = create_age_gender_table<double>(age_range);
		auto count = create_age_gender_table<double>(age_range);
		for (auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			auto combine_risk = calculate_combined_relative_risk(
				entity, context.start_time(), context.time_now());
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
						sum(age, core::Gender::female) / female_count;
				}
			}
		}
	}

	void DefaultCancerModel::update_disease_status(RuntimeContext& context) {
		// Order is very important!
		update_remission_cases(context);
		update_incidence_cases(context);
	}

	double DefaultCancerModel::get_excess_mortality(const Person& entity) const noexcept {
		auto disease_info = entity.diseases.at(disease_type());
		auto max_onset = definition_.get().parameters().max_time_since_onset;
		if (disease_info.time_since_onset < 0 || disease_info.time_since_onset >= max_onset) {
			return 0.0;
		}
		
		auto mortality_id = definition_.get().table().at("mortality");
		auto excess_mortality = definition_.get().table()(entity.age, entity.gender).at(mortality_id);
		auto death_weight = definition_.get().parameters().death_weight.at(disease_info.time_since_onset);
		if (entity.gender == core::Gender::male) {
			return excess_mortality * death_weight.males;
		}

		return excess_mortality * death_weight.females;
	}

	DoubleAgeGenderTable DefaultCancerModel::calculate_average_relative_risk(RuntimeContext& context) {
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

	double DefaultCancerModel::calculate_combined_relative_risk(
		const Person& entity, const int& start_time, const int& time_now) const {
		auto combined_risk_value = 1.0;
		combined_risk_value *= calculate_relative_risk_for_risk_factors(entity);
		combined_risk_value *= calculate_relative_risk_for_diseases(entity, start_time, time_now);
		return combined_risk_value;
	}

	double DefaultCancerModel::calculate_relative_risk_for_risk_factors(const Person& entity) const {
		auto relative_risk_value = 1.0;
		auto& relative_factors = definition_.get().relative_risk_factors();
		for (auto& factor : entity.risk_factors) {
			if (!relative_factors.contains(factor.first)) {
				continue;
			}

			auto& lut = relative_factors.at(factor.first).at(entity.gender);
			auto entity_factor_value = static_cast<float>(factor.second);
			auto relative_factor_value = lut(entity.age, entity_factor_value);
			relative_risk_value *= relative_factor_value;
		}

		return relative_risk_value;
	}

	double DefaultCancerModel::calculate_relative_risk_for_diseases(
		const Person& entity, const int& start_time, const int& time_now) const {
		auto relative_risk_value = 1.0;
		auto& lut = definition_.get().relative_risk_diseases();
		for (auto& disease : entity.diseases) {
			if (!lut.contains(disease.first)) {
				continue;
			}

			// Only include existing diseases
			if (disease.second.status == DiseaseStatus::active &&
				(start_time == time_now || disease.second.start_time < time_now)) {
				auto relative_disease_vale = lut.at(disease.first)(entity.age, entity.gender);

				relative_risk_value *= relative_disease_vale;
			}
		}

		return relative_risk_value;
	}

	void DefaultCancerModel::update_remission_cases(RuntimeContext& context) {
		auto remission_count = 0;
		auto prevalence_count = 0;
		auto max_onset = definition_.get().parameters().max_time_since_onset;
		for (auto& entity : context.population()) {
			if (entity.age == 0 || !entity.is_active()) {
				continue;
			}

			if (!entity.diseases.contains(disease_type()) ||
				entity.diseases.at(disease_type()).status != DiseaseStatus::active) {
				continue;
			}

			// Increment duration by one year
			entity.diseases.at(disease_type()).time_since_onset++;
			if (entity.diseases.at(disease_type()).time_since_onset >= max_onset) {
				entity.diseases.at(disease_type()).status = DiseaseStatus::free;
				entity.diseases.at(disease_type()).time_since_onset = -1;
				remission_count++;
			}

			prevalence_count++;
		}

		// Debug calculation
		auto remission_percent = remission_count * 100.0 / prevalence_count;
	}

	void DefaultCancerModel::update_incidence_cases(RuntimeContext& context) {
		auto incidence_count = 0;
		auto prevalence_count = 0;
		auto population_count = 0;
		for (auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			if (entity.age == 0) {
				if (entity.diseases.size() > 0) {
					entity.diseases.clear(); // Should not have nay disease at birth!
				}

				continue;
			}

			population_count++;

			// Already have disease
			if (entity.diseases.contains(disease_type()) &&
				entity.diseases.at(disease_type()).status == DiseaseStatus::active) {
				prevalence_count++;
				continue;
			}

			auto probability = calculate_incidence_probability(
				entity, context.start_time(), context.time_now());
			auto hazard = context.random().next_double();
			if (hazard < probability) {
				entity.diseases[disease_type()] = Disease{
									.status = DiseaseStatus::active,
									.start_time = context.time_now(),
									.time_since_onset = 0 };
				incidence_count++;
				prevalence_count++;
			}
		}

		// Debug calculation
		auto disease_incidence = incidence_count * 100.0 / population_count;
		auto disease_prevalence = prevalence_count * 100.0 / population_count;
	}

	double DefaultCancerModel::calculate_incidence_probability(
		const Person& entity, const int& start_time, const int& time_now) const {
		auto incidence_id = definition_.get().table().at("incidence");
		auto combined_relative_risk = calculate_combined_relative_risk(entity, start_time, time_now);
		auto average_relative_risk = average_relative_risk_.at(entity.age, entity.gender);
		auto incidence = definition_.get().table()(entity.age, entity.gender).at(incidence_id);
		auto probability = incidence * combined_relative_risk / average_relative_risk;
		return probability;
	}

	int DefaultCancerModel::calculate_time_since_onset(
		RuntimeContext& context, const core::Gender& gender) const {

		auto& pdf = definition_.get().parameters().prevalence_distribution;
		auto values = std::vector<int>{};
		auto cumulative = std::vector<double>{};
		auto sum = 0.0;
		for (auto& item : pdf) {
			auto p = item.second.males;
			if (gender != core::Gender::male) {
				p = item.second.females;
			}

			sum += p;
			values.emplace_back(item.first);
			cumulative.emplace_back(sum);
		}

		return context.random().next_empirical_discrete(values, cumulative);
	}
}