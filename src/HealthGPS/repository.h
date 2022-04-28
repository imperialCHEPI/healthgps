#pragma once
#include "interfaces.h"
#include "hierarchical_model_types.h"
#include "modelinput.h"
#include "disease_definition.h"
#include <mutex>
#include <functional>
#include <optional>

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

		virtual LiteHierarchicalModelDefinition& get_lite_linear_model_definition(
			const HierarchicalModelType& model_type) = 0;

		virtual const std::vector<core::DiseaseInfo>& get_diseases() = 0;

		virtual std::optional<core::DiseaseInfo> get_disease_info(std::string code) = 0;

		virtual DiseaseDefinition& get_disease_definition(
			const core::DiseaseInfo& info, const ModelInput& config) = 0;
	};

	class CachedRepository final: public Repository {
	public:
		CachedRepository() = delete;
		CachedRepository(core::Datastore& manager);

		bool register_linear_model_definition(
			const HierarchicalModelType& model_type,
			HierarchicalLinearModelDefinition&& definition);

		bool register_lite_linear_model_definition(
			const HierarchicalModelType& model_type,
			LiteHierarchicalModelDefinition&& definition);

		HierarchicalLinearModelDefinition& get_linear_model_definition(
			const HierarchicalModelType& model_type) override;

		LiteHierarchicalModelDefinition& get_lite_linear_model_definition(
			const HierarchicalModelType& model_type) override;

		const std::vector<core::DiseaseInfo>& get_diseases() override;

		std::optional<core::DiseaseInfo> get_disease_info(std::string code) override;

		DiseaseDefinition& get_disease_definition(
			const core::DiseaseInfo& info, const ModelInput& config) override;

		core::Datastore& manager() noexcept override;

		void clear_cache() noexcept;

	private:
		std::mutex mutex_;
		std::reference_wrapper<core::Datastore> data_manager_;
		std::map<HierarchicalModelType, HierarchicalLinearModelDefinition> model_definiton_;
		std::map<HierarchicalModelType, LiteHierarchicalModelDefinition> lite_model_definiton_;
		std::vector<core::DiseaseInfo> diseases_info_;
		std::map<std::string, DiseaseDefinition> diseases_;

		void load_disease_definition(const core::DiseaseInfo& info, const ModelInput& config);
	};
}
