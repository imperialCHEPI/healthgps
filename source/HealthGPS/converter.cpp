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

			return DiseaseTable(entity.info.name, std::move(measures), std::move(data));
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
			auto relative_diseases = std::map<std::string, FloatAgeGenderTable>();
			for (auto& item : info.inputs.diseases()) {
				auto table = info.manager.get_relative_risk_to_disease(info.disease, item);
				if (!table.empty()) {
					relative_diseases.emplace(item.code, StoreConverter::to_relative_risk_table(table));
				}
			}

			auto relative_factors = std::map<std::string, std::map<core::Gender, RelativeRiskLookup>>();
			for (auto& factor : info.risk_factors) {
				auto table_male = info.manager.get_relative_risk_to_risk_factor(
					info.disease, core::Gender::male, factor.key());
				auto table_feme = info.manager.get_relative_risk_to_risk_factor(
					info.disease, core::Gender::female, factor.key());

				if (!table_male.empty()) {
					relative_factors[factor.key()].emplace(
						core::Gender::male, StoreConverter::to_relative_risk_lookup(table_male));
				}

				if (!table_feme.empty()) {
					relative_factors[factor.key()].emplace(
						core::Gender::female, StoreConverter::to_relative_risk_lookup(table_feme));
				}
			}

			return RelativeRisk(std::move(relative_diseases), std::move(relative_factors));
		}
	}
}
