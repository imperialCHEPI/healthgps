#pragma once

#include "default_disease_model.h"
#include "default_cancer_model.h"

namespace hgps {
	namespace detail {
		/// @brief Defines the disease model builder function signature and required parameters type
		using DiseaseModelBuilder = std::function<std::shared_ptr<DiseaseModel>(
			DiseaseDefinition& definition, WeightModel&& classifier, const core::IntegerInterval& age_range)>;
	}

	/// @brief Create the default disease model builder registry for disease group.
	/// @return the default production instance of the disease model builder registry.
	std::map<core::DiseaseGroup, detail::DiseaseModelBuilder> get_default_disease_model_registry() {
		auto registry = std::map<core::DiseaseGroup, detail::DiseaseModelBuilder> {
			{ core::DiseaseGroup::other, [](DiseaseDefinition& definition, WeightModel&& classifier,
				const core::IntegerInterval& age_range) {
					return std::make_shared<DefaultDiseaseModel>(definition, std::move(classifier), age_range); }},

			{ core::DiseaseGroup::cancer, [](DiseaseDefinition& definition, WeightModel&& classifier,
				const core::IntegerInterval& age_range) {
					return std::make_shared<DefaultCancerModel>(definition, std::move(classifier), age_range); }},
		};

		return registry;
	}
}
