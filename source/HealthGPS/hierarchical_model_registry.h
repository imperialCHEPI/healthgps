#pragma once
#include "hierarchical_model.h"

#include <memory>

namespace hgps {
	namespace detail {
		using HierarchicalModelBuilder = std::function<std::unique_ptr<HierarchicalLinearModel>(
			HierarchicalLinearModelDefinition& definition)>;

		using LiteHierarchicalModelBuilder = std::function<std::unique_ptr<HierarchicalLinearModel>(
			LiteHierarchicalModelDefinition& definition)>;
	}

	std::map<HierarchicalModelType, detail::HierarchicalModelBuilder> get_default_hierarchical_model_registry() {
		auto registry = std::map<HierarchicalModelType, detail::HierarchicalModelBuilder>{
			{HierarchicalModelType::Static, [](HierarchicalLinearModelDefinition& definition) {
				return std::make_unique<StaticHierarchicalLinearModel>(definition); }},
		};

		return registry;
	}

	std::map<HierarchicalModelType, detail::LiteHierarchicalModelBuilder> get_default_lite_hierarchical_model_registry() {
		auto registry = std::map<HierarchicalModelType, detail::LiteHierarchicalModelBuilder>{
			{HierarchicalModelType::Dynamic, [](LiteHierarchicalModelDefinition& definition) {
				return std::make_unique<EnergyBalanceHierarchicalModel>(definition); }},
		};

		return registry;
	}
}
