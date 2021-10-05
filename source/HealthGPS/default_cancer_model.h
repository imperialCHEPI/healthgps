#pragma once
#include "interfaces.h"
#include "disease_definition.h"
#include "gender_table.h"

namespace hgps {

	class DefaultCancerModel final : public DiseaseModel {
	public:
		DefaultCancerModel() = delete;
		DefaultCancerModel(DiseaseDefinition&& definition, const core::IntegerInterval age_range);

		core::DiseaseGroup group() const noexcept override;

		std::string disease_type() const noexcept override;

		void initialise_disease_status(RuntimeContext& context) override;

		void initialise_average_relative_risk(RuntimeContext& context) override;

		void update_disease_status(RuntimeContext& context) override;

		double get_excess_mortality(const Person& entity) const noexcept override;

		// Need to find a better way, possible injecting the survival function!!!
		static double get_survival_rate(const ParameterLookup& survival,
			const core::Gender& gender, const int& age, const int& time_year);

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
		int calculate_time_since_onset(RuntimeContext& context, const core::Gender& gender) const;
	};
}