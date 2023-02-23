#pragma once
#include <map>
#include <vector>
#include <string>
#include "forward_type.h"

namespace hgps {
	namespace core {

		/// @brief Life expectancy item for a country data structure
		struct LifeExpectancyItem
		{
			/// @brief Reference time
			int at_time{};

			/// @brief Both sexes life expectancy
			float both{};

			/// @brief Male life expectancy
			float male{};

			/// @brief Male life expectancy
			float female{};
		};

		/// @brief Burden of Diseases (BoD) analysis for a country data structure
		struct DiseaseAnalysisEntity
		{
			/// @brief Diseases disability weight value
			std::map<std::string, float> disability_weights{};

			/// @brief Collection of life expectancy indicators
			std::vector<LifeExpectancyItem> life_expectancy{};

			/// @brief The cost of disease lookup table by age and gender
			std::map<int, std::map<Gender, double>> cost_of_diseases{};

			/// @brief Checks whether the disease analysis definition is empty
			/// @return true for an empty definition, otherwise false.
			bool empty() const noexcept {
				return cost_of_diseases.empty() || life_expectancy.empty();
			}
		};

		/// @brief LMS (lambda-mu-sigma) parameters data structure
		struct LmsDataRow
		{
			/// @brief Reference age
			int age{};

			/// @brief Reference gender identifier
			Gender gender{};

			/// @brief Lambda parameter value
			double lambda{};

			/// @brief Mu parameter value
			double mu{};

			/// @brief Sigma parameter value
			double sigma{};
		};
	}
}
