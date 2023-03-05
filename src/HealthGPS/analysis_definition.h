#pragma once
#include "gender_table.h"
#include "HealthGPS.Core/identifier.h"

#include <map>
#include <string>


namespace hgps {

	/// @brief Burden of diseases (BoD) analysis module definition data type.
	class AnalysisDefinition {
	public:
		/// @brief Initialises a new instance of the AnalysisDefinition class.
		/// @param life_expectancy Population life expectancy table
		/// @param observed_YLD Observed years lived with disability (YLD) table
		/// @param disability_weights Diseases disability weights
		AnalysisDefinition(GenderTable<int, float>&& life_expectancy,
			DoubleAgeGenderTable&& observed_YLD, std::map<core::Identifier, float>&& disability_weights)
			: life_expectancy_{ std::move(life_expectancy) }, observed_YLD_{ std::move(observed_YLD) }, 
			disability_weights_{ std::move(disability_weights) }
		{}

		/// @brief Gets the population life expectancy table
		/// @return Life expectancy table
		const GenderTable<int, float>& life_expectancy() const noexcept {
			return life_expectancy_;
		}

		/// @brief Gets the observed years lived with disability (YLD) table
		/// @return Observed YLD table
		const DoubleAgeGenderTable& observed_YLD() const noexcept {
			return observed_YLD_;
		}

		/// @brief Gets the diseases disability weights
		/// @return Disability weights
		const std::map<core::Identifier, float>& disability_weights() const noexcept {
			return disability_weights_;
		}

	private:
		GenderTable<int, float> life_expectancy_;
		DoubleAgeGenderTable observed_YLD_;
		std::map<core::Identifier, float> disability_weights_;
	};
}