#pragma once

#include <map>
#include <string>
#include "gender_table.h"

namespace hgps {
	class AnalysisDefinition {
	public:
		AnalysisDefinition(GenderTable<int, float>&& life_expectancy,
			DoubleAgeGenderTable&& observed_YLD, std::map<std::string, float>&& disability_weights)
			: life_expectancy_{ life_expectancy }, observed_YLD_{ observed_YLD }, 
			disability_weights_{ disability_weights }
		{}

		const GenderTable<int, float>& life_expectancy() const noexcept {
			return life_expectancy_;
		}

		const DoubleAgeGenderTable& observed_YLD() const noexcept {
			return observed_YLD_;
		}

		const std::map<std::string, float> disability_weights() const noexcept {
			return disability_weights_;
		}

	private:
		GenderTable<int, float> life_expectancy_;
		DoubleAgeGenderTable observed_YLD_;
		std::map<std::string, float> disability_weights_;
	};
}