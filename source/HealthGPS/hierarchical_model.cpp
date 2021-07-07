#include "hierarchical_model.h"

namespace hgps {
	HierarchicalLinearModel::HierarchicalLinearModel(
		std::unordered_map<std::string, LinearModel>&& models,
		std::map<int, HierarchicalLevel>&& levels) 
		: models_{ models }, levels_{ levels }
	{}

	HierarchicalModelType HierarchicalLinearModel::type() const {
		return HierarchicalModelType::Static;
	}

	std::string HierarchicalLinearModel::name() const {
		return "Static";
	}

	void HierarchicalLinearModel::generate(RuntimeContext& context) {
		throw std::logic_error("Function not implemented.");
	}

	// ************ Dynamic Hierarchical Model ************

	DynamicHierarchicalLinearModel::DynamicHierarchicalLinearModel(
		std::unordered_map<std::string, LinearModel>&& models,
		std::map<int, HierarchicalLevel>&& levels) 
		: HierarchicalLinearModel(std::move(models), std::move(levels))
	{}

	HierarchicalModelType DynamicHierarchicalLinearModel::type() const {
		return HierarchicalModelType::Dynamic;
	}

	std::string DynamicHierarchicalLinearModel::name() const {
		return "Dynamic";
	}

	void DynamicHierarchicalLinearModel::generate(RuntimeContext& context) {
		throw std::logic_error("Function not implemented.");
	}
}
