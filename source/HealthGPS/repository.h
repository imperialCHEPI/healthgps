#pragma once
#include "interfaces.h"
#include "hierarchical_model_types.h"
#include <mutex>

namespace hgps {

	class Repository {
	public:
		Repository() = default;
		Repository(Repository&&) = delete;
		Repository(const Repository&) = delete;
		Repository& operator=(Repository&&) = delete;
		Repository& operator=(const Repository&) = delete;
		virtual ~Repository() = default;

		virtual core::Datastore& manager() noexcept = 0;

		virtual HierarchicalLinearModelDefinition& get_linear_model_definition(
			const HierarchicalModelType& model_type) = 0;
	};

	class CachedRepository final: public Repository {
	public:
		CachedRepository() = delete;
		CachedRepository(core::Datastore& manager);

		bool register_linear_model_definition(
			const HierarchicalModelType& model_type,
			HierarchicalLinearModelDefinition&& definition);

		HierarchicalLinearModelDefinition& get_linear_model_definition(
			const HierarchicalModelType& model_type) override;

		core::Datastore& manager() noexcept override;

		void clear_cache() noexcept;

	private:
		std::mutex mutex_;
		core::Datastore& data_manager_;
		std::map<HierarchicalModelType, HierarchicalLinearModelDefinition> model_definiton_;
	};
}
