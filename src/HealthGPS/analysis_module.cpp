
#include "analysis_module.h"
#include "weight_model.h"
#include "lms_model.h"
#include "converter.h"

#include <cmath>

namespace hgps {
	AnalysisModule::AnalysisModule(
		AnalysisDefinition&& definition, WeightModel&& classifier, const core::IntegerInterval age_range)
		: definition_{ std::move(definition) }, weight_classifier_{std::move(classifier)}
		, residual_disability_weight_{ create_age_gender_table<double>(age_range)}
		, channels_{}
	{}

	SimulationModuleType AnalysisModule::type() const noexcept {
		return SimulationModuleType::Analysis;
	}

	std::string AnalysisModule::name() const noexcept {
		return "Analysis";
	}

	void AnalysisModule::initialise_population(RuntimeContext& context) {
		auto& age_range = context.age_range();
		auto expected_sum = create_age_gender_table<double>(age_range);
		auto expected_count = create_age_gender_table<int>(age_range);

		for (const auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			auto sum = 1.0;
			for (const auto& disease : entity.diseases) {
				if (disease.second.status == DiseaseStatus::active &&
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

		initialise_output_channels(context);

		publish_result_message(context);
	}

	void AnalysisModule::update_population(RuntimeContext& context) {
		publish_result_message(context);
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

	void AnalysisModule::publish_result_message(RuntimeContext& context) const {
		auto result = calculate_historical_statistics(context);

		calculate_population_statistics(context, result.time_series);

		context.publish(std::make_unique<ResultEventMessage>(
			context.identifier(), context.current_run(), context.time_now(), result));
	}

	ModelResult AnalysisModule::calculate_historical_statistics(RuntimeContext& context) const {
		auto risk_factors = std::map<std::string, std::map<core::Gender, double>>();
		for (const auto& item : context.mapping()) {
			if (item.level() > 0) {
				risk_factors.emplace(item.key(), std::map<core::Gender, double>{});
			}
		}

		auto prevalence = std::map<std::string, std::map<core::Gender, int>>();
		for (const auto& item : context.diseases()) {
			prevalence.emplace(item.code, std::map<core::Gender, int>{});
		}

		auto age_sum = std::map<core::Gender, int>{};
		auto age_count = std::map<core::Gender, int>{};
		auto analysis_time = static_cast<unsigned int>(context.time_now());
		auto population_size = static_cast<int>(context.population().size());
		auto population_alive = 0;
		auto population_dead = 0;
		auto population_migrated = 0;
		for (const auto& entity : context.population()) {
			if (!entity.is_active()) {
				if (entity.is_alive()) {
					if (entity.time_of_migration() == analysis_time) {
						population_migrated++;
					}
				}
				else if (entity.time_of_death() == analysis_time) {
					population_dead++;
				}

				continue;
			}

			population_alive++;
			age_sum[entity.gender] += entity.age;
			age_count[entity.gender]++;
			for (auto& item : risk_factors) {
				auto factor_value = entity.get_risk_factor_value(item.first);
				if (std::isnan(factor_value)) {
					factor_value = 0.0;
				}

				item.second[entity.gender] += factor_value;
			}

			for (const auto& item : context.diseases()) {
				if (entity.diseases.contains(item.code) && 
					entity.diseases.at(item.code).status == DiseaseStatus::active) {
						prevalence.at(item.code)[entity.gender]++;
				}
			}
		}

		// Calculate the averages avoiding division by zero
		auto sample_size = context.age_range().upper() + 1u;
		auto result = ModelResult{ sample_size };
		result.population_size = population_size;
		result.number_alive = population_alive;
		result.number_dead = population_dead;
		result.number_emigrated = population_migrated;
		auto males_count = std::max(1, age_count[core::Gender::male]);
		auto females_count = std::max(1, age_count[core::Gender::female]);
		result.average_age.male = age_sum[core::Gender::male] * 1.0 / males_count;
		result.average_age.female = age_sum[core::Gender::female] * 1.0 / females_count;
		for (auto& item : risk_factors) {
			auto user_name = context.mapping().at(item.first).name();
			result.risk_ractor_average.emplace(user_name, ResultByGender {
					.male = item.second[core::Gender::male] / males_count,
					.female = item.second[core::Gender::female] / females_count
				});
		}

		for (const auto& item : context.diseases()) {
			result.disease_prevalence.emplace(item.code, ResultByGender {
					.male = prevalence.at(item.code)[core::Gender::male] * 100.0 / males_count,
					.female = prevalence.at(item.code)[core::Gender::female] * 100.0 / females_count
				});
		}

		for (const auto& item : context.metrics()) {
			result.metrics.emplace(item.first, item.second);
		}

		result.indicators = calculate_dalys(context.population(), context.age_range().upper(), analysis_time);
		return result;
	}

	double AnalysisModule::calculate_disability_weight(const Person& entity) const {
		auto sum = 1.0;
		for (const auto& disease : entity.diseases) {
			if (disease.second.status == DiseaseStatus::active) {
				if (definition_.disability_weights().contains(disease.first)) {
					sum *= (1.0 - definition_.disability_weights().at(disease.first));
				}
			}
		}

		auto residual_dw = residual_disability_weight_.at(entity.age, entity.gender);
		residual_dw = std::min(1.0, std::max(residual_dw, 0.0));
		sum *= (1.0 - residual_dw);
		return 1.0 - sum;
	}

	DALYsIndicator AnalysisModule::calculate_dalys(Population& population,
		const unsigned int& max_age, const unsigned int& death_year) const {
		auto yll_sum = 0.0;
		auto yld_sum = 0.0;
		auto count = 0;
		for (const auto& entity : population) {
			if (entity.time_of_death() == death_year) {
				if (entity.age <= max_age) {
					auto male_reference_age = definition_.life_expectancy().at(death_year, core::Gender::male);
					auto female_reference_age = definition_.life_expectancy().at(death_year, core::Gender::female);

					auto reference_age = std::max(male_reference_age, female_reference_age);
					auto lifeExpectancy = std::max(reference_age - entity.age, 0.0f);
					yll_sum += lifeExpectancy;
				}
			}

			if (entity.is_active()){
				yld_sum += calculate_disability_weight(entity);
				count++;
			}
		}

		auto yll = yll_sum * 100000.0 / count;
		auto yld = yld_sum * 100000.0 / count;
		return DALYsIndicator
		{
			.years_of_life_lost = yll,
			.years_lived_with_disability = yld,
			.disablity_adjusted_life_years = yll + yld
		};
	}

	void hgps::AnalysisModule::calculate_population_statistics(RuntimeContext& context, DataSeries& series) const
	{
		using namespace core;

		auto min_age = context.age_range().lower();
		auto max_age = context.age_range().upper();
		if (series.size() > 0) {
			throw std::logic_error("This should be a new object!");
		}

		series.add_channels(channels_);

		auto daly_units = 100'000.0;
		auto current_time = static_cast<unsigned int>(context.time_now());
		for (const auto & entity : context.population()) {
			auto age = entity.age;
			auto gender = entity.gender;

			if (!entity.is_active()) {
				if (!entity.is_alive() && entity.time_of_death() == current_time) {
					series(gender, "deaths").at(age)++;
					auto expcted_life = definition_.life_expectancy().at(context.time_now(), gender);
					series(gender,"yll").at(age) += std::max(expcted_life - age, 0.0f) * daly_units;
				}

				if (entity.has_emigrated() && entity.time_of_migration() == current_time) {
					series(gender, "migrations").at(age)++;
				}

				continue;
			}

			series(gender, "count").at(age)++;
			for (auto& factor : context.mapping().entries()) {
				series(gender,factor.key()).at(age) += entity.get_risk_factor_value(factor.key());
			}

			for (auto& item : entity.diseases) {
				if (item.second.status == DiseaseStatus::active) {
					series(gender, item.first).at(age)++;
				}
			}

			auto dw = calculate_disability_weight(entity);
			series(gender,"disability_weight").at(age) += dw;
			series(gender,"yld").at(age) += (dw * daly_units);

			classify_weight(series, entity);
		}

		// Calculate DALY
		for (auto idx = min_age; idx <= max_age; idx++) {
			series(Gender::male, "daly").at(idx) = series(Gender::male, "yll").at(idx) + series(Gender::male, "yld").at(idx);
			series(Gender::female, "daly").at(idx) = series(Gender::female, "yll").at(idx) + series(Gender::female, "yld").at(idx);
		}

		// Calculate in-place averages
		for (auto index = min_age; index <= max_age; index++) {
			for (auto& chan : series.channels()) {
				if (chan == "count") {
					continue;
				}

				auto real_count = series(Gender::male, "count").at(index);
				if (real_count > 0.0) {
					series(Gender::male, chan).at(index) = series(Gender::male, chan).at(index) / real_count;
				}
				else {
					series(Gender::male, chan).at(index) = 0.0;
				}

				real_count = series(Gender::female, "count").at(index);
				if (real_count > 0.0) {
					series(Gender::female,chan).at(index) = series(Gender::female, chan).at(index) / real_count;
				}
				else {
					series(Gender::female, chan).at(index) = 0.0;
				}
			}
		}
	}

	void AnalysisModule::classify_weight(hgps::DataSeries& series, const hgps::Person& entity) const
	{
		auto weight_class = weight_classifier_.classify_weight(entity);
		switch (weight_class)
		{
		case hgps::WeightCategory::normal:
			series(entity.gender, "normal_weight").at(entity.age)++;
			break;
		case hgps::WeightCategory::overweight:
			series(entity.gender, "over_weight").at(entity.age)++;
			series(entity.gender, "above_weight").at(entity.age)++;
			break;
		case hgps::WeightCategory::obese:
			series(entity.gender, "obese_weight").at(entity.age)++;
			series(entity.gender, "above_weight").at(entity.age)++;
			break;
		default:
			throw std::logic_error("Unknow weight classification category.");
			break;
		}
	}

	void AnalysisModule::initialise_output_channels(RuntimeContext& context)
	{
		if (!channels_.empty()) {
			return;
		}

		channels_.push_back("count");
		for (auto& factor : context.mapping().entries_without_dynamic()) {
			channels_.emplace_back(factor.key());
		}

		for (auto& disease : context.diseases()) {
			channels_.emplace_back(disease.code);
		}

		channels_.emplace_back("disability_weight");
		channels_.emplace_back("deaths");
		channels_.emplace_back("migrations");
		channels_.emplace_back("normal_weight");
		channels_.emplace_back("over_weight");
		channels_.emplace_back("obese_weight");
		channels_.emplace_back("above_weight");
		channels_.emplace_back("yll");
		channels_.emplace_back("yld");
		channels_.emplace_back("daly");
	}

	std::unique_ptr<AnalysisModule> build_analysis_module(Repository& repository, const ModelInput& config)
	{
		auto analysis_entity = repository.manager().get_disease_analysis(config.settings().country());
		if (analysis_entity.empty()) {
			throw std::logic_error(
				"Failed to create analysis module, invalid disease analysis definition.");
		}

		auto definition = detail::StoreConverter::to_analysis_definition(analysis_entity);
		auto lms_definition = detail::StoreConverter::to_lms_definition(analysis_entity.lms_parameters);
		auto classifier = WeightModel{ LmsModel{ std::move(lms_definition) } };

		return std::make_unique<AnalysisModule>(
			std::move(definition), std::move(classifier), config.settings().age_range());
	}
}
