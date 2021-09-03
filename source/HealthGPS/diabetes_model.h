#pragma once
#include "interfaces.h"
#include "disease_definition.h"
#include "gender_table.h"

namespace hgps {
	class DiabetesModel final : public DiseaseModel {
	public:
		DiabetesModel() = delete;
		DiabetesModel(DiseaseDefinition&& definition, const core::IntegerInterval age_range);

		std::string disease_type() const noexcept override;

		void initialise_disease_status(RuntimeContext& context) override;

		void initialise_average_relative_risk(RuntimeContext& context) override;

		void update_disease_status(RuntimeContext& context) override;

		double get_excess_mortality(const int& age, const core::Gender& gender) const noexcept override;

	private:
		DiseaseDefinition definition_;
		DoubleAgeGenderTable average_relative_risk_;

		DoubleAgeGenderTable calculate_average_relative_risk(RuntimeContext& context);
		double calculate_combined_relative_risk(const Person& entity, const int& time_now) const;
		double calculate_relative_risk_for_diseases(const Person& entity, const int& time_now) const;
		double calculate_relative_risk_for_risk_factors(const Person& entity) const;
		double calculate_incidence_probability(const Person& entity, const int& time_now) const;

		void update_remission_cases(RuntimeContext& context);
		void update_incidence_cases(RuntimeContext& context);
	};
}

