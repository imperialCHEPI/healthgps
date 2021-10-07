#include "converter.h"

#include "HealthGPS.Core/string_util.h"
#include "default_cancer_model.h"

namespace hgps {
	namespace detail {
		core::Gender StoreConverter::to_gender(std::string name)
		{
			if (core::case_insensitive::equals(name, "male")) {
				return core::Gender::male;
			}

			if (core::case_insensitive::equals(name, "female")) {
				return core::Gender::female;
			}

			return core::Gender::unknown;
		}

		DiseaseTable StoreConverter::to_disease_table(const core::DiseaseEntity& entity)
		{
			auto measures = entity.measures;
			auto data = std::map<int, std::map<core::Gender, DiseaseMeasure>>();
			for (auto& v : entity.items) {
				data[v.age][v.gender] = DiseaseMeasure(v.measures);
			}

			return DiseaseTable(entity.info, std::move(measures), std::move(data));
		}

		DiseaseTable StoreConverter::to_disease_table(const core::DiseaseEntity& entity,
			const DiseaseParameter& parameter, const core::IntegerInterval& age_range)
		{
			// TODO: Add missing to configuration
			auto remission_id = 7;
			auto mortality_id = 15;
			auto smooth_times = 50;
			auto measures = std::map<std::string, int>{ entity.measures };
			measures.emplace("remission", remission_id);
			measures.emplace("mortality", mortality_id);

			// Create local editable disease table, populate missing measures
			auto data = std::map<int, std::map<core::Gender, std::map<int, double>>>();
			for (auto& v : entity.items) {
				data[v.age][v.gender] = std::map<int, double>{ v.measures };

				auto mortality = DefaultCancerModel::get_survival_rate(
					parameter.survival_rate, v.gender, v.age, parameter.time_year);
				data[v.age][v.gender].emplace(remission_id, 0.0);
				data[v.age][v.gender].emplace(mortality_id, 1.0 - mortality);
			}

			// Smooth rates
			for (auto& item : measures) {
				if (item.first == "incidence") {
					continue;
				}

				smooth_rates(smooth_times, data, item.second);
			}

			update_incidence(data, measures, age_range);
			auto table = std::map<int, std::map<core::Gender, DiseaseMeasure>>();
			for (auto& item : data) {
				table[item.first][core::Gender::male] = DiseaseMeasure(item.second.at(core::Gender::male));
				table[item.first][core::Gender::female] = DiseaseMeasure(item.second.at(core::Gender::female));
			}

			return DiseaseTable(entity.info, std::move(measures), std::move(table));
		}

		void update_incidence(std::map<int, std::map<hgps::core::Gender, std::map<int, double>>>& data,
			const std::map<std::string, int>& measures, const hgps::core::IntegerInterval& age_range)
		{
			// Update incident, overwrite loaded data???
			auto prevalence_id = measures.at("prevalence");
			auto incidence_id = measures.at("incidence");
			auto remission_id = measures.at("remission"); 
			auto mortality_id = measures.at("mortality");
			const auto start_age = age_range.lower() + 1;
			for (auto age = start_age; age <= age_range.upper(); age++) {
				auto p_male = age == start_age ? 0.0 : data[age - 1][core::Gender::male].at(prevalence_id);
				auto& male_measure = data[age][core::Gender::male];
				auto male_value = 1.0 - male_measure.at(remission_id) - male_measure.at(mortality_id);
				male_value = (male_measure.at(prevalence_id) - male_value * p_male) / (1.0 - p_male);
				male_measure.at(incidence_id) = std::max(std::min(male_value, 1.0), 0.0);

				auto p_female = age == start_age ? 0.0 : data[age - 1][core::Gender::female].at(prevalence_id);
				auto& female_measure = data[age][core::Gender::female];
				auto female_value = 1.0 - female_measure.at(remission_id) - female_measure.at(mortality_id);
				female_value = (female_measure.at(prevalence_id) - female_value * p_female) / (1.0 - p_female);
				female_measure.at(incidence_id) = std::max(std::min(female_value, 1.0), 0.0);
			}
		}

		void smooth_rates(const int& times, std::map<int,
			std::map<hgps::core::Gender, std::map<int, double>>>& data, const int& measure_id)
		{
			if (data.size() < 2) {
				return;
			}

			auto start_age = data.cbegin()->first;
			auto count = data.size() + start_age;
			auto count_minus_one = count - 1;
			auto male_res = std::vector<double>(count);
			auto female_res = std::vector<double>(count);
			for (auto& item : data) {
				male_res[item.first] = item.second.at(core::Gender::male).at(measure_id);
				female_res[item.first] = item.second.at(core::Gender::female).at(measure_id);
			}

			const auto divisor = 3.0;
			for (auto j = 0; j < times; j++) {
				auto male_tmp = male_res;
				auto female_tmp = female_res;

				for (std::size_t i = 0; i < count; i++) {
					if (i == 0) {
						male_res[i] = (2.0 * male_tmp[i] + male_tmp[i + 1]) / divisor;
						female_res[i] = (2.0 * female_tmp[i] + female_tmp[i + 1]) / divisor;
					}
					else if (i == count_minus_one){
						male_res[i] = (male_tmp[i - 1] + male_tmp[i] * 2.0) / divisor;
						female_res[i] = (female_tmp[i - 1] + female_tmp[i] * 2.0) / divisor;
					}
					else {
						male_res[i] = (male_tmp[i - 1] + male_tmp[i] + male_tmp[i + 1]) / divisor;
						female_res[i] = (female_tmp[i - 1] + female_tmp[i] + female_tmp[i + 1]) / divisor;
					}
				}
			}

			auto index = start_age;
			for (auto& item : data) {
				item.second.at(core::Gender::male).at(measure_id) = male_res[index];
				item.second.at(core::Gender::female).at(measure_id) = female_res[index];
				index++;
			}
		}

		FloatAgeGenderTable StoreConverter::to_relative_risk_table(const core::RelativeRiskEntity& entity)
		{
			auto num_rows = entity.rows.size();
			auto num_cols = entity.columns.size() - 1;
			auto cols = std::vector<core::Gender>();
			for (size_t i = 1; i <= num_cols; i++) {
				auto gender = to_gender(entity.columns[i]);
				if (gender == core::Gender::unknown) {
					throw std::out_of_range(
						std::format("Invalid column gender type: {}", entity.columns[i]));
				}

				cols.emplace_back(gender);
			}

			auto rows = std::vector<int>(num_rows);
			auto data = core::FloatArray2D(num_rows, num_cols);
			for (size_t i = 0; i < num_rows; i++) {
				auto row = entity.rows[i];
				rows[i] = static_cast<int>(row[0]);
				for (size_t j = 1; j < row.size(); j++) {
					data(i, j - 1) = row[j];
				}
			}

			return FloatAgeGenderTable(MonotonicVector(rows), cols, std::move(data));
		}

		RelativeRiskLookup StoreConverter::to_relative_risk_lookup(const core::RelativeRiskEntity& entity)
		{
			auto num_rows = entity.rows.size();
			auto num_cols = entity.columns.size() - 1;
			auto cols = std::vector<float>();
			for (size_t i = 1; i <= num_cols; i++) {
				cols.emplace_back(std::stof(entity.columns[i]));
			}

			auto rows = std::vector<int>(num_rows);
			auto data = core::FloatArray2D(num_rows, num_cols);
			for (size_t i = 0; i < num_rows; i++) {
				auto row = entity.rows[i];
				rows[i] = static_cast<int>(row[0]);
				for (size_t j = 1; j < row.size(); j++) {
					data(i, j - 1) = row[j];
				}
			}

			return RelativeRiskLookup(MonotonicVector(rows), MonotonicVector(cols), std::move(data));
		}

		RelativeRisk create_relative_risk(RelativeRiskInfo info)
		{
			RelativeRisk result;
			for (auto& item : info.inputs.diseases()) {
				auto table = info.manager.get_relative_risk_to_disease(info.disease, item);
				if (!table.empty()) {
					result.diseases.emplace(item.code, StoreConverter::to_relative_risk_table(table));
				}
			}

			for (auto& factor : info.risk_factors) {
				auto table_male = info.manager.get_relative_risk_to_risk_factor(
					info.disease, core::Gender::male, factor.key());
				auto table_feme = info.manager.get_relative_risk_to_risk_factor(
					info.disease, core::Gender::female, factor.key());

				if (!table_male.empty()) {
					result.risk_factors[factor.key()].emplace(
						core::Gender::male, StoreConverter::to_relative_risk_lookup(table_male));
				}

				if (!table_feme.empty()) {
					result.risk_factors[factor.key()].emplace(
						core::Gender::female, StoreConverter::to_relative_risk_lookup(table_feme));
				}
			}

			return result;
		}

		AnalysisDefinition StoreConverter::to_analysis_definition(
			const core::DiseaseAnalysisEntity& entity, const core::IntegerInterval& age_range)
		{
			auto cols = std::vector<core::Gender>{ core::Gender::male, core::Gender::female };
			auto lex_rows = std::vector<int>(entity.life_expectancy.size());
			auto lex_data = core::FloatArray2D(lex_rows.size(), cols.size());
			for (int index = 0; index < entity.life_expectancy.size(); index++) {
				const auto& item = entity.life_expectancy.at(index);
				lex_rows[index] = item.time;
				lex_data(index, 0) = item.male;
				lex_data(index, 1) = item.female;
			}

			auto life_expectancy = GenderTable<int, float>(MonotonicVector(lex_rows), cols, std::move(lex_data));

			auto min_time = entity.cost_of_diseases.begin()->first;
			auto max_time = entity.cost_of_diseases.rbegin()->first;
			auto cost_of_disease = create_age_gender_table<double>(core::IntegerInterval(min_time, max_time));
			for (const auto& item : entity.cost_of_diseases) {
				cost_of_disease(item.first, core::Gender::male) = item.second.at(core::Gender::male);
				cost_of_disease(item.first, core::Gender::female) = item.second.at(core::Gender::female);
			}

			auto weights = std::map<std::string, float>(entity.disability_weights);
			return AnalysisDefinition(std::move(life_expectancy), std::move(cost_of_disease), std::move(weights));
		}

		LifeTable StoreConverter::to_life_table(std::vector<core::BirthItem>& births,
			std::vector<core::MortalityItem>& deaths)
		{
			auto table_births = std::map<int, Birth>{};
			for (auto& item : births) {
				table_births.emplace(item.time, Birth{ item.number, item.sex_ratio });
			}

			auto table_deaths = std::map<int, std::map<int, Mortality>>{};
			for (auto& item : deaths) {
				table_deaths[item.year].emplace(item.age, Mortality(item.males, item.females));
			}

			return LifeTable(std::move(table_births), std::move(table_deaths));
		}
		
		DiseaseParameter StoreConverter::to_disease_parameter(const core::CancerParameterEntity entity)
		{
			auto distribution = ParameterLookup{};
			for (auto& item : entity.distribution) {
				distribution.emplace(item.value, DoubleGenderValue(item.male, item.female));
			}

			auto survival = ParameterLookup{};
			for (auto& item : entity.survival_rate) {
				survival.emplace(item.value, DoubleGenderValue(item.male, item.female));
			}

			// Make sure that the deaths table is zero based!
			auto deaths = ParameterLookup{};
			auto offset = entity.death_weight.front().value;
			for (auto& item : entity.death_weight) {
				deaths.emplace(item.value - offset, DoubleGenderValue(item.male, item.female));
			}

			return DiseaseParameter(entity.time_year, distribution, survival, deaths);
		}
	}
}
