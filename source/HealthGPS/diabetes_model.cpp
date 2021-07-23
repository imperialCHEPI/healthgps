#include "diabetes_model.h"
#include "runtime_context.h"

namespace hgps {
	DiabetesModel::DiabetesModel(std::string identifier, DiseaseTable&& table)
		: identifier_{ identifier }, table_{ table } {}

	std::string DiabetesModel::type() const noexcept { return identifier_; }

	void DiabetesModel::generate(RuntimeContext& context) 
	{
		auto summary = std::map<int, std::map<core::Gender, std::pair<double, int>>>();
		for (auto& entity : context.population()) {
			if (!entity.is_active()) {
				continue;
			}

			auto combine_risk = calculate_combined_relative_risk(entity);
			auto pair = summary[entity.age][entity.gender];
			pair.first += combine_risk;
			pair.second++;
		}
	}

	void DiabetesModel::adjust_relative_risk(RuntimeContext& context) {
		throw std::logic_error("DiabetesModel.adjust_relative_risk function not yet implemented.");
	}

	void DiabetesModel::update(RuntimeContext& context) {
		throw std::logic_error("DiabetesModel.update function not yet implemented.");
	}

	double DiabetesModel::calculate_combined_relative_risk(Person& entity)
	{
		// The modelled risk factors
		double combinedRelativeRisk = 1.0;
		for (auto& factor : entity.risk_factors) {
			auto factor_value = entity.get_risk_factor_value(factor.first);
			combinedRelativeRisk *= factor_value;
		}

		return 0.0;
	}
}
