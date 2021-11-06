#pragma once

#include "default_disease_model.h"
#include "default_cancer_model.h"

namespace hgps {
	namespace detail {
		using DiseaseModelBuilder = std::function<std::shared_ptr<DiseaseModel>(
			DiseaseDefinition&& definition, const core::IntegerInterval age_range)>;
	}

	std::map<core::DiseaseGroup, detail::DiseaseModelBuilder> get_default_disease_model_registry() {
		auto registry = std::map<core::DiseaseGroup, detail::DiseaseModelBuilder>{
			{core::DiseaseGroup::other, [](DiseaseDefinition&& definition, const core::IntegerInterval age_range) {
				return std::make_shared<DefaultDiseaseModel>(std::move(definition), age_range); }},

			{core::DiseaseGroup::cancer, [](DiseaseDefinition&& definition, const core::IntegerInterval age_range) {
				return std::make_shared<DefaultCancerModel>(std::move(definition), age_range); }},
		};

		return registry;
	}
}
