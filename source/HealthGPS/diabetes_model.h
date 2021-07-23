#pragma once
#include "interfaces.h"
#include "disease_table.h"

namespace hgps {
	class DiabetesModel final : public DiseaseModel {
	public:
		DiabetesModel() = delete;
		DiabetesModel(std::string identifier, DiseaseTable&& table);

		std::string type() const noexcept override;

		void generate(RuntimeContext& context) override;

		void adjust_relative_risk(RuntimeContext& context) override;

		void update(RuntimeContext& context) override;

	private:
		std::string identifier_;
		DiseaseTable table_;
		std::map<int, std::map<core::Gender, double>> relative_risk_;

		double calculate_combined_relative_risk(Person& entity);
	};
}

