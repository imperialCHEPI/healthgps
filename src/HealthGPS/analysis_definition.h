#pragma once
#include "gender_table.h"
#include "HealthGPS.Core/identifier.h"

#include <map>
#include <string>


namespace hgps {

	class AnalysisDefinition {
	public:
		AnalysisDefinition(GenderTable<int, float>&& life_expectancy,
			DoubleAgeGenderTable&& observed_YLD, std::map<core::Identifier, float>&& disability_weights)
			: life_expectancy_{ std::move(life_expectancy) }, observed_YLD_{ std::move(observed_YLD) }, 
			disability_weights_{ std::move(disability_weights) }
		{}

		const GenderTable<int, float>& life_expectancy() const noexcept {
			return life_expectancy_;
		}

		const DoubleAgeGenderTable& observed_YLD() const noexcept {
			return observed_YLD_;
		}

		const std::map<core::Identifier, float>& disability_weights() const noexcept {
			return disability_weights_;
		}

	private:
		GenderTable<int, float> life_expectancy_;
		DoubleAgeGenderTable observed_YLD_;
		std::map<core::Identifier, float> disability_weights_;
	};
}