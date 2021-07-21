#include "diabetes_model.h"

namespace hgps {
	DiabetesModel::DiabetesModel(std::string identifier, DiseaseTable&& table)
		: identifier_{ identifier }, table_{ table } {}

	std::string DiabetesModel::type() const noexcept { return identifier_; }

	void DiabetesModel::generate(RuntimeContext& context) {
		throw std::logic_error("DiabetesModel.generate function not yet implemented.");
	}

	void DiabetesModel::adjust_relative_risk(RuntimeContext& context) {
		throw std::logic_error("DiabetesModel.adjust_relative_risk function not yet implemented.");
	}

	void DiabetesModel::update(RuntimeContext& context) {
		throw std::logic_error("DiabetesModel.update function not yet implemented.");
	}
}
