#pragma once

#include "hierarchical_model.h"
#include "repository.h"
#include "modelinput.h"

namespace hgps {

	class RiskFactorModule final : public SimulationModule {
	public:
		RiskFactorModule() = delete;
		RiskFactorModule(std::unordered_map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>&& models);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		std::size_t size() const noexcept;

		bool contains(const HierarchicalModelType& modelType) const noexcept;

		HierarchicalLinearModel& at(const HierarchicalModelType& model_type) const;

		void initialise_population(RuntimeContext& context) override;

		void update_population(RuntimeContext& context);

	private:
		std::unordered_map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>> models_;
	};

	std::unique_ptr<RiskFactorModule> build_risk_factor_module(Repository& repository, const ModelInput& config);
}