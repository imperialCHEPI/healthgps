#pragma once

#include "diabetes_model.h"

namespace hgps {
	namespace detail {
		using DiseaseModelBuilder = std::function<std::shared_ptr<DiseaseModel>(
			DiseaseDefinition&& definition, const core::IntegerInterval age_range)>;
	}

	std::map<std::string, detail::DiseaseModelBuilder> get_default_disease_model_registry() {
		auto registry = std::map<std::string, detail::DiseaseModelBuilder>{
				{"diabetes", [](DiseaseDefinition&& definition, const core::IntegerInterval age_range) {
					return std::make_shared<DiabetesModel>(std::move(definition), age_range); }}
		};

		return registry;
	}
}
