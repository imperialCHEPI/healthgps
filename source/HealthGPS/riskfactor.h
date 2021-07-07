#pragma once

#include "hierarchical_model.h"

namespace hgps {

	class RiskFactorModule final : public SimulationModule {
	public:
		RiskFactorModule() = delete;
		RiskFactorModule(std::unordered_map<HierarchicalModelType, HierarchicalLinearModel>&& models);

		SimulationModuleType type() const override;

		std::string name() const override;

		std::size_t size() const noexcept;

		HierarchicalLinearModel& operator[](HierarchicalModelType modelType);

		const HierarchicalLinearModel& operator[](HierarchicalModelType modelType) const;

		void initialise_population(RuntimeContext& context);

	private:
		std::unordered_map<HierarchicalModelType, HierarchicalLinearModel> models_;
	};
}