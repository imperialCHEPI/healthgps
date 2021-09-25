#include "model_data_provider.h"

namespace hgps {

	ModelDataProvider::ModelDataProvider(core::Datastore& manager)
		: data_manager_{ manager }, model_definiton_{} {}

	void ModelDataProvider::register_linear_model_definitions(HLMDefinitionMap&& definitions) {
		std::unique_lock<std::mutex> lock(mutex_);
		model_definiton_ = std::move(definitions);
	}

	HierarchicalLinearModelDefinition& ModelDataProvider::get_linear_model_definition(
		const HierarchicalModelType& model_type) {

		std::unique_lock<std::mutex> lock(mutex_);
		return model_definiton_.at(model_type);
	}

	core::Datastore& ModelDataProvider::manager() noexcept {
		return data_manager_;
	}

	void ModelDataProvider::clear_cache() noexcept {
		std::unique_lock<std::mutex> lock(mutex_);
		model_definiton_.clear();
	}
}
