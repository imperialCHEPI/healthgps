#pragma once
#include "interfaces.h"
#include "disease_table.h"
#include "relative_risk.h"

namespace hgps {
	class DiabetesModel final : public DiseaseModel {
	public:
		DiabetesModel() = delete;
		DiabetesModel(std::string identifier, DiseaseTable&& table, RelativeRisk&& risks);

		std::string type() const noexcept override;

		void generate(RuntimeContext& context) override;

		void adjust_relative_risk(RuntimeContext& context) override;

		void update(RuntimeContext& context) override;

	private:
		std::string identifier_;
		DiseaseTable table_;
		RelativeRisk risks_;

		double calculate_combined_relative_risk(Person& entity);
	};
}

