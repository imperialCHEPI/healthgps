#pragma once

#include <string>
#include<vector>
#include <map>

#include "forward_type.h"
#include "identifier.h"

namespace hgps {
	namespace core {

		struct DiseaseInfo
		{
			DiseaseGroup group{};
			Identifier code{};
			std::string name{};
		};

		inline bool operator> (const DiseaseInfo& lhs, const DiseaseInfo& rhs) {
			return lhs.name > rhs.name;
		}

		inline bool operator< (const DiseaseInfo& lhs, const DiseaseInfo& rhs) {
			return lhs.name < rhs.name;
		}

		struct DiseaseItem
		{
			int age;
			Gender gender;
			std::map<int, double> measures;
		};

		struct DiseaseEntity
		{
			Country country;
			DiseaseInfo info;
			std::map<std::string, int> measures;
			std::vector<DiseaseItem> items;

			bool empty() const noexcept {
				return measures.empty() || items.empty();
			}
		};

		struct RelativeRiskEntity
		{
			bool is_default_value{};
			std::vector<std::string> columns;
			std::vector<std::vector<float>> rows;

			bool empty() const noexcept { return rows.empty(); }
		};

		struct CancerParameterEntity
		{
			int time_year{};
			std::vector<LookupGenderValue> death_weight{};
			std::vector<LookupGenderValue> prevalence_distribution{};
			std::vector<LookupGenderValue> survival_rate{};
		};
	}
}
