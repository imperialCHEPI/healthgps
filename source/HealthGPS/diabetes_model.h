#pragma once
#include "interfaces.h"
#include "disease_definition.h"
#include "gender_table.h"

namespace hgps {
	class DiabetesModel final : public DiseaseModel {
	public:
		DiabetesModel() = delete;
		DiabetesModel(DiseaseDefinition&& definition, const core::IntegerInterval age_range);

		std::string type() const noexcept override;

		void initialise_disease_status(RuntimeContext& context) override;

		void initialise_average_relative_risk(RuntimeContext& context) override;

		void update(RuntimeContext& context) override;

	private:
		DiseaseDefinition definition_;
		DoubleAgeGenderTable average_relative_risk_;

		DoubleAgeGenderTable calculate_average_relative_risk(RuntimeContext& context);
		double calculate_combined_relative_risk(Person& entity, const int time_now);
		double calculate_relative_risk_for_diseases(Person& entity, const int time_now);
		double calculate_relative_risk_for_risk_factors(Person& entity);
	};
}

