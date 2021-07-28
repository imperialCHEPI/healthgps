#pragma once
#include "interfaces.h"
#include "disease_table.h"
#include "relative_risk.h"
#include "gender_table.h"

namespace hgps {
	class DiabetesModel final : public DiseaseModel {
	public:
		DiabetesModel() = delete;
		DiabetesModel(std::string identifier, DiseaseTable&& table,
			RelativeRisk&& risks, core::IntegerInterval age_range);

		std::string type() const noexcept override;

		void initialise_disease_status(RuntimeContext& context) override;

		void initialise_average_relative_risk(RuntimeContext& context) override;

		void update(RuntimeContext& context) override;

	private:
		std::string identifier_;
		DiseaseTable measures_table_;
		RelativeRisk relative_risks_;
		DoubleAgeGenderTable average_relative_risk_;

		DoubleAgeGenderTable calculate_average_relative_risk(RuntimeContext& context);
		double calculate_combined_relative_risk(Person& entity, const int time_now);
		double calculate_relative_risk_for_diseases(Person& entity, const int time_now);
		double calculate_relative_risk_for_risk_factors(Person& entity);
	};
}

