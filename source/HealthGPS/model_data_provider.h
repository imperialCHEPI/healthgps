#pragma once
#include "interfaces.h"
#include "hierarchical_model_types.h"
#include <mutex>

namespace hgps {

	class ModelDataProvider {
	public:
		ModelDataProvider() = delete;
		ModelDataProvider(core::Datastore& manager);

		void register_linear_model_definitions(HLMDefinitionMap&& definitions);

		HierarchicalLinearModelDefinition& get_linear_model_definition(
			const HierarchicalModelType& model_type);

		core::Datastore& manager() noexcept;

		void clear_cache() noexcept;

	private:
		std::mutex mutex_;
		core::Datastore& data_manager_;
		HLMDefinitionMap model_definiton_;
	};
}
