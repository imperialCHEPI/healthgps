#pragma once
#include "hierarchical_model_static.h"
#include "hierarchical_model_dynamic.h"

#include <memory>

namespace hgps {
	namespace detail {
		using HierarchicalModelBuilder = std::function<std::unique_ptr<HierarchicalLinearModel>(
			HierarchicalLinearModelDefinition& definition)>;
	}

	std::map<HierarchicalModelType, detail::HierarchicalModelBuilder> get_default_hierarchical_model_registry() {
		auto registry = std::map<HierarchicalModelType, detail::HierarchicalModelBuilder>{
			{HierarchicalModelType::Static, [](HierarchicalLinearModelDefinition& definition) {
				return std::make_unique<StaticHierarchicalLinearModel>(definition); }},

			{HierarchicalModelType::Dynamic, [](HierarchicalLinearModelDefinition& definition) {
				return std::make_unique<DynamicHierarchicalLinearModel>(definition); }},
		};

		return registry;
	}
}
