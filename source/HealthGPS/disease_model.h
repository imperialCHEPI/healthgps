#pragma once

#include "diabetes_model.h"

namespace hgps {
	namespace detail {
		using DiseaseModelBuilder = std::function<std::shared_ptr<DiseaseModel>(
			const std::string, DiseaseTable&& data, RelativeRisk&& risks)>;
	}

	std::map<std::string, detail::DiseaseModelBuilder> get_default_disease_model_registry() {
		auto registry = std::map<std::string, detail::DiseaseModelBuilder>{
				{"diabetes", [](const std::string name, DiseaseTable&& data, RelativeRisk&& risks) {
					return std::make_shared<DiabetesModel>(name, std::move(data), std::move(risks)); }}
		};

		return registry;
	}
}
