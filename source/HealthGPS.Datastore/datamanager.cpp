#include <fstream>
#include <rapidcsv.h>

#include "datamanager.h"
#include "HealthGPS.Core/string_util.h"

namespace hgps {
	namespace data {
		DataManager::DataManager(const std::filesystem::path root_directory)
			: root_{ root_directory }
		{
			// TODO: use precondition contract
			std::ifstream ifs(root_ / "index.json", std::ifstream::in);
			if (ifs) {
				index_ = nlohmann::json::parse(ifs);
			}
		}

		std::vector<Country> DataManager::get_countries() const
		{
			auto results = std::vector<Country>();
			if (index_.contains("country")) {
				auto filepath = index_["country"]["path"].get<std::string>();
				auto filename = index_["country"]["file_name"].get<std::string>();
				filename = (root_ / filepath / filename).string();

				// TODO: use precondition contract
				if (std::filesystem::exists(filename)) {
					rapidcsv::Document doc(filename);
					for (size_t i = 0; i < doc.GetRowCount(); i++) {
						auto row = doc.GetRow<std::string>(i);
						results.push_back(Country
							{
								.code = std::stoi(row[0]),
								.name = row[1],
								.alpha2 = row[2],
								.alpha3 = row[3]
							});
					}

					std::sort(results.begin(), results.end());
				}
			}

			return results;
		}

		std::optional<Country> DataManager::get_country(std::string alpha) const
		{
			// TODO: use caching or create a parser with filter
			auto v = get_countries();
			auto is_target = [&alpha](const hgps::core::Country& obj) {
				return core::case_insensitive::equals(obj.alpha2, alpha) ||
					core::case_insensitive::equals(obj.alpha3, alpha);
			};

			if (auto it = std::find_if(v.begin(), v.end(), is_target); it != v.end()) {
				return (*it);
			}

			return std::nullopt;
		}

		std::vector<PopulationItem> DataManager::get_population(Country country) const {
			return  DataManager::get_population(country, [](const unsigned int& value) {return true; });
		}

		std::vector<PopulationItem> DataManager::get_population(
			Country country, const std::function<bool(const unsigned int&)> year_filter) const {
			auto results = std::vector<PopulationItem>();

			if (index_.contains("population")) {
				auto filepath = index_["population"]["path"].get<std::string>();
				auto filename = index_["population"]["file_name"].get<std::string>();

				// Tokenized file names X{country.code}X.xxx
				filename = replace_string_tokens(filename, { std::to_string(country.code) });
				filename = (root_ / filepath / filename).string();

				// LocID,Location,VarID,Variant,Time,MidPeriod,AgeGrp,AgeGrpStart,AgeGrpSpan,PopMale,PopFemale,PopTotal
				if (std::filesystem::exists(filename)) {
					rapidcsv::Document doc(filename);

					for (size_t i = 0; i < doc.GetRowCount(); i++) {
						auto row = doc.GetRow<std::string>(i);
						auto time_year = std::stoi(row[4]);
						if (!year_filter(time_year)) {
							continue;
						}

						results.push_back(PopulationItem
							{
								.code = std::stoi(row[0]),		// LocID
								.year = time_year,				// Time
								.age = std::stoi(row[6]),		// AgeGrp
								.males = std::stof(row[9]),		// PopMale
								.females = std::stof(row[10]),	// PopFemale
								.total = std::stof(row[11])		// PopTotal
							});
					}

					std::sort(results.begin(), results.end());
				}
			}

			return results;
		}

		std::vector<DiseaseInfo> DataManager::get_diseases() const
		{
			auto result = std::vector<DiseaseInfo>();
			if (index_.contains("diseases")) {
				auto types = index_["diseases"]["types"].get<std::map<std::string, std::string>>();
				for (auto& item : types) {
					result.emplace_back(DiseaseInfo{
						.code = item.first,
						.name = item.second
						});
				}

				std::sort(result.begin(), result.end());
			}

			return result;
		}

		std::optional<DiseaseInfo> DataManager::get_disease_info(std::string code) const
		{
			if (index_.contains("diseases")) {
				auto types = index_["diseases"]["types"].get<std::map<std::string, std::string>>();
				auto disease_code = core::to_lower(code);
				if (types.contains(disease_code)) {
					return DiseaseInfo{
						.code = disease_code,
						.name = types.at(disease_code)
					};
				}
			}

			return std::optional<DiseaseInfo>();
		}

		DiseaseEntity DataManager::get_disease(DiseaseInfo info, Country country) const
		{
			DiseaseEntity result;
			result.info = info;
			result.country = country;

			if (index_.contains("diseases")) {
				auto filepath = index_["diseases"]["path"].get<std::string>();
				auto filename = index_["diseases"]["file_name"].get<std::string>();

				// Tokenized file names X{country.code}X.xxx
				filename = replace_string_tokens(filename, { std::to_string(country.code) });

				filename = (root_ / filepath / info.code / filename).string();
				if (std::filesystem::exists(filename)) {
					rapidcsv::Document doc(filename);

					auto table = std::unordered_map<int,
						std::unordered_map<core::Gender, std::map<int, double>>>();

					for (size_t i = 0; i < doc.GetRowCount(); i++) {
						auto row = doc.GetRow<std::string>(i);
						auto age = std::stoi(row[6]);
						auto gender = static_cast<core::Gender>(std::stoi(row[8]));
						auto measure_id = std::stoi(row[10]);
						auto measure_name = core::to_lower(row[11]);
						auto measure_value = std::stod(row[12]);

						if (!result.measures.contains(measure_name)) {
							result.measures.emplace(measure_name, measure_id);
						}

						if (!table[age].contains(gender)) {
							table[age].emplace(gender, std::map<int, double>{});
						}

						table[age][gender][measure_id] = measure_value;
					}

					for (auto& pair : table) {
						for (auto& child : pair.second) {
							result.items.emplace_back(DiseaseItem{
								.age = pair.first,
								.gender = child.first,
								.measures = child.second
								});
						}
					}

					result.items.shrink_to_fit();
				}
			}

			return result;
		}

		RelativeRiskTable DataManager::get_relative_risk_to_disease(
			DiseaseInfo source, DiseaseInfo target) const
		{
			if (index_.contains("diseases")) {
				auto disease_folder = index_["diseases"]["path"].get<std::string>();
				auto risk_node = index_["diseases"]["relative_risk"];
				auto risk_folder = risk_node["path"].get<std::string>();
				auto file_folder = risk_node["to_disease"]["path"].get<std::string>();
				auto filename = risk_node["to_disease"]["file_name"].get<std::string>();

				// Tokenized file name{ DISEASE_TYPE }_{ DISEASE_TYPE }.xxx
				auto tokens = { source.code, target.code };
				filename = replace_string_tokens(filename, tokens);

				filename = (root_ / disease_folder / source.code / risk_folder / file_folder / filename).string();
				if (std::filesystem::exists(filename)) {
					rapidcsv::Document doc(filename);

					auto table = RelativeRiskTable();
					table.is_default_value = false;
					table.columns = doc.GetColumnNames();

					auto row_size = table.columns.size();
					for (size_t i = 0; i < doc.GetRowCount(); i++) {
						auto row = doc.GetRow<std::string>(i);

						auto age = std::stoi(row[0]);
						auto age_row = std::vector<float>(row_size - 1);
						for (size_t col = 1; col < row_size; col++) {
							age_row[col - 1] = std::stof(row[col]);
						}

						table.rows.emplace(age, age_row);
					}

					return table;
				}

				return generate_default_relative_risk_to_disease();
			}

			return RelativeRiskTable();
		}

		RelativeRiskTable DataManager::get_relative_risk_to_risk_factor(
			DiseaseInfo source, Gender gender, std::string risk_factor) const
		{
			auto table = RelativeRiskTable();
			if (!index_.contains("diseases")) {
				return table;
			}

			// Creates full file name from store configuration
			auto disease_folder = index_["diseases"]["path"].get<std::string>();
			auto risk_node = index_["diseases"]["relative_risk"];
			auto risk_folder = risk_node["path"].get<std::string>();
			auto file_folder = risk_node["to_risk_factor"]["path"].get<std::string>();
			auto filename = risk_node["to_risk_factor"]["file_name"].get<std::string>();

			std::string gender_name = gender == Gender::male ? "male" : "female";

			// Tokenized file name {GENDER}_{DISEASE_TYPE}_{RISK_FACTOR}.xxx
			auto tokens = { gender_name, source.code, risk_factor };
			filename = replace_string_tokens(filename, tokens);

			filename = (root_ / disease_folder / source.code / risk_folder / file_folder / filename).string();
			if (!std::filesystem::exists(filename)) {
				return table;
			}

			rapidcsv::Document doc(filename);

			table.is_default_value = false;
			table.columns = doc.GetColumnNames();
			auto row_size = table.columns.size();
			for (size_t i = 0; i < doc.GetRowCount(); i++) {
				auto row = doc.GetRow<std::string>(i);

				auto age = std::stoi(row[0]);
				auto age_row = std::vector<float>(row_size - 1);
				for (size_t col = 1; col < row_size; col++) {
					age_row[col - 1] = std::stof(row[col]);
				}

				table.rows.emplace(age, age_row);
			}

			return table;
		}

		RelativeRiskTable DataManager::generate_default_relative_risk_to_disease() const
		{
			if (index_.contains("diseases")) {
				auto age_limits = index_["diseases"]["age_limits"].get<std::vector<int>>();
				auto to_disease = index_["diseases"]["relative_risk"]["to_disease"];
				auto default_value = to_disease["default_value"].get<float>();

				auto table = RelativeRiskTable();
				table.is_default_value = true;
				table.columns = { "age","male","female" };
				for (int age = age_limits[0]; age <= age_limits[1]; age++) {
					table.rows.emplace(age, std::vector<float>(2, default_value));
				}

				return table;
			}

			return RelativeRiskTable();
		}

		std::string DataManager::replace_string_tokens(std::string source, std::vector<std::string> tokens) const
		{
			std::string output = source;
			std::size_t tk_end = 0;
			for (auto& tk : tokens) {
				auto tk_start = output.find_first_of("{", tk_end);
				if (tk_start != std::string::npos) {
					tk_end = output.find_first_of("}", tk_start + 1);
					if (tk_end != std::string::npos) {
						output = output.replace(tk_start, tk_end - tk_start + 1, tk);
						tk_end = tk_start + tk.length();
					}
				}
			}

			return output;
		}
	}
}