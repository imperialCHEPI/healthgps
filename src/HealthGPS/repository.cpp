#include "repository.h"

namespace hgps {

	CachedRepository::CachedRepository(core::Datastore& manager)
		: data_manager_{ manager }, model_definiton_{} {}

	bool CachedRepository::register_linear_model_definition(
		const HierarchicalModelType& model_type, HierarchicalLinearModelDefinition&& definition) {
		std::unique_lock<std::mutex> lock(mutex_);
		if (model_definiton_.contains(model_type)) {
			model_definiton_.erase(model_type);
		}

		auto success = model_definiton_.emplace(model_type, std::move(definition));
		return success.second;
	}

	bool CachedRepository::register_lite_linear_model_definition(
		const HierarchicalModelType& model_type, LiteHierarchicalModelDefinition&& definition) {
		std::unique_lock<std::mutex> lock(mutex_);
		if (lite_model_definiton_.contains(model_type)) {
			lite_model_definiton_.erase(model_type);
		}

		auto success = lite_model_definiton_.emplace(model_type, std::move(definition));
		return success.second;
	}

	HierarchicalLinearModelDefinition& CachedRepository::get_linear_model_definition(
		const HierarchicalModelType& model_type) {

		std::scoped_lock<std::mutex> lock(mutex_);
		return model_definiton_.at(model_type);
	}

	LiteHierarchicalModelDefinition& CachedRepository::get_lite_linear_model_definition(
		const HierarchicalModelType& model_type) {

		std::scoped_lock<std::mutex> lock(mutex_);
		return lite_model_definiton_.at(model_type);
	}

	core::Datastore& CachedRepository::manager() noexcept {
		return data_manager_;
	}

	void CachedRepository::clear_cache() noexcept {
		std::unique_lock<std::mutex> lock(mutex_);
		model_definiton_.clear();
	}
}
