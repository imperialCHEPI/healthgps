#pragma once

#include "hierarchical_model.h"

namespace hgps {

	class RiskFactorModule final : public SimulationModule {
	public:
		RiskFactorModule() = delete;
		RiskFactorModule(std::unordered_map<HierarchicalModelType, std::shared_ptr<HierarchicalLinearModel>>&& models);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		std::size_t size() const noexcept;

		std::shared_ptr<HierarchicalLinearModel> operator[](HierarchicalModelType modelType);

		const std::shared_ptr<HierarchicalLinearModel> operator[](HierarchicalModelType modelType) const;

		void initialise_population(RuntimeContext& context) override;

	private:
		std::unordered_map<HierarchicalModelType, std::shared_ptr<HierarchicalLinearModel>> models_;
	};
}