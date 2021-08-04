#pragma once
#include <map>
#include <vector>
#include <string>
#include "forward_type.h"

namespace hgps {
	namespace core {

		struct LifeExpectancyItem
		{
			int time;
			float both;
			float male;
			float female;
		};

		struct DiseaseAnalysisEntity
		{
			std::map<std::string, float> disability_weights{};
			std::vector<LifeExpectancyItem> life_expectancy{};
			std::map<int, std::map<Gender, double>> cost_of_diseases{};

			bool empty() const noexcept {
				return cost_of_diseases.empty() || life_expectancy.empty();
			}
		};
	}
}
