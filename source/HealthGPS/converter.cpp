#include "converter.h"

#include "HealthGPS.Core/string_util.h"

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
	}
}