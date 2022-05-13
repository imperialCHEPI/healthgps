#pragma once

#include "runtime_context.h"
#include "riskfactor_adjustment_types.h"

#include <functional>

namespace hgps {

	class RiskfactorAdjustmentModel
	{
	public:
		RiskfactorAdjustmentModel() = delete;
		RiskfactorAdjustmentModel(BaselineAdjustment& adjustments);

		void Apply(RuntimeContext& context);

	private:
		std::reference_wrapper<BaselineAdjustment> adjustments_;

		FactorAdjustmentTable calculate_simulated_mean(Population& population,
			const core::IntegerInterval& age_range) const;

		FactorAdjustmentTable calculate_adjustment_coefficients(RuntimeContext& context) const;

		FactorAdjustmentTable get_adjustment_coefficients(RuntimeContext& context) const;
	};
}
