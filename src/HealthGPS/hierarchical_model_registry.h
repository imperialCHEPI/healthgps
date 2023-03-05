#pragma once
#include "hierarchical_model.h"

#include <memory>

namespace hgps {
	namespace detail {
		/// @brief Defines the full hierarchical model builder equation
		using HierarchicalModelBuilder = std::function<std::unique_ptr<HierarchicalLinearModel>(
			HierarchicalLinearModelDefinition& definition)>;

		/// @brief Defines the lite hierarchical model builder equation
		using LiteHierarchicalModelBuilder = std::function<std::unique_ptr<HierarchicalLinearModel>(
			LiteHierarchicalModelDefinition& definition)>;
	}

	/// @brief Gets the default production instance builders for full hierarchical regression models
	/// @return The hierarchical models builder functions registry
	std::map<HierarchicalModelType, detail::HierarchicalModelBuilder> get_default_hierarchical_model_registry() {
		auto registry = std::map<HierarchicalModelType, detail::HierarchicalModelBuilder>{
			{HierarchicalModelType::Static, [](HierarchicalLinearModelDefinition& definition) {
				return std::make_unique<StaticHierarchicalLinearModel>(definition); }},
		};

		return registry;
	}

	/// @brief Gets the default production instance builders for lite hierarchical regression models
	/// @return The hierarchical models builder functions registry
	std::map<HierarchicalModelType, detail::LiteHierarchicalModelBuilder> get_default_lite_hierarchical_model_registry() {
		auto registry = std::map<HierarchicalModelType, detail::LiteHierarchicalModelBuilder>{
			{HierarchicalModelType::Dynamic, [](LiteHierarchicalModelDefinition& definition) {
				return std::make_unique<EnergyBalanceHierarchicalModel>(definition); }},
		};

		return registry;
	}
}
