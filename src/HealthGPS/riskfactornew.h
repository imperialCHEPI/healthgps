#pragma once

#include "hierarchical_model.h"
#include "riskfactor_adjustment.h"
#include "repository.h"
#include "modelinput.h"

namespace hgps {

	/// @brief Defines the risk factors module container to hold hierarchical linear models.
	class RiskFactorModuleNew final : public RiskFactorHostModule {
	public:
		RiskFactorModuleNew() = delete;

		/// @brief Initialises a new instance of the RiskFactorModuleNew class.
		/// @param models Collection of hierarchical linear model instances
		/// @param adjustments The baseline risk factor adjustments values
		/// @throws std::invalid_argument for empty hierarchical models collection or missing required module type.
		/// @throws std::out_of_range for model type and model instance type mismatch.
		RiskFactorModuleNew(std::map<HierarchicalModelType,
				    std::unique_ptr<HierarchicalLinearModel>>&& models,
				    RiskfactorAdjustmentModel&& adjustments);

		SimulationModuleType type() const noexcept override;

		const std::string& name() const noexcept override;

		std::size_t size() const noexcept override;

		bool contains(const HierarchicalModelType& modelType) const noexcept override;

		HierarchicalLinearModel& at(const HierarchicalModelType& model_type) const;

		void initialise_population(RuntimeContext& context) override;

		void update_population(RuntimeContext& context) override;

		void apply_baseline_adjustments(RuntimeContext& context) override;

	private:
		std::map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>> models_;
		RiskfactorAdjustmentModel adjustment_;
		std::string name_{ "RiskFactor" };
	};

	std::unique_ptr<RiskFactorModuleNew> build_risk_factor_module(Repository& repository, const ModelInput& config);
}
