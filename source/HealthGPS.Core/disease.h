#pragma once

#include <string>
#include<vector>
#include <map>

#include "forward_type.h"

namespace hgps {
	namespace core {

		struct DiseaseInfo
		{
			std::string code{};
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
		};

		struct RelativeRiskTable
		{
			bool is_default_value{};
			std::vector<std::string> columns;
			std::map<int, std::vector<float>> rows;

			bool empty() const noexcept { return rows.empty(); }
		};
	}
}
