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
				auto tk_start = filename.find_first_of("{");
				if (tk_start != std::string::npos) {
					auto tk_end = filename.find_first_of("}", tk_start +1);
					if (tk_end != std::string::npos) {
						filename = filename.replace(tk_start, tk_end - tk_start + 1, std::to_string(country.code));
					}
				}

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
				auto types = index_["diseases"]["types"].get<std::map<std::string,std::string>>();
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
				auto tk_start = filename.find_first_of("{");
				if (tk_start != std::string::npos) {
					auto tk_end = filename.find_first_of("}", tk_start + 1);
					if (tk_end != std::string::npos) {
						filename = filename.replace(tk_start, tk_end - tk_start + 1, std::to_string(country.code));
					}
				}

				filename = (root_ / filepath / info.code / filename).string();
				if (std::filesystem::exists(filename)) {
					rapidcsv::Document doc(filename);

					auto table = std::unordered_map<int,
						std::unordered_map<core::Gender, std::unordered_map<int, double>>>();

					for (size_t i = 0; i < doc.GetRowCount(); i++) {
						auto row = doc.GetRow<std::string>(i);
						auto age = std::stoi(row[6]);								// age
						auto gender = static_cast<core::Gender>(std::stoi(row[8]));	// gender_id
						auto measure_id = std::stoi(row[10]);						// measure_id
						auto measure_key = core::to_lower(row[11]);					// measure
						auto measure_value = std::stod(row[12]);					// mean

						if (!result.measures.contains(measure_key)) {
							result.measures.emplace(measure_key, measure_id);
						}

						if (!table[age].contains(gender)) {
							table[age].emplace(gender, std::unordered_map<int, double>{});
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
	}
}