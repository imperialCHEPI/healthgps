#pragma once

#include "map2d.h"
#include "runtime_context.h"

#include "risk_factor_adjustable_model.h"

namespace hgps {

/// @brief Defines the risk factor baseline adjustment data type
struct BaselineAdjustment final {
    /// @brief Initialises a new instance of the BaselineAdjustment structure
    BaselineAdjustment() = default;

    /// @brief Initialises a new instance of the BaselineAdjustment structure
    /// @param adjustment_table The baseline adjustment table
    /// @throws std::invalid_argument for empty adjustment table or table missing a gender entry
    BaselineAdjustment(RiskFactorSexAgeTable &&adjustment_table)
        : values{std::move(adjustment_table)} {

        if (values.empty()) {
            throw std::invalid_argument("The risk factors adjustment table must not be empty.");
        } else if (values.rows() != 2) {
            throw std::out_of_range(
                "The risk factors adjustment definition must contain two tables.");
        }

        if (!values.contains(core::Gender::male)) {
            throw std::invalid_argument("Missing the required adjustment table for male.");
        } else if (!values.contains(core::Gender::female)) {
            throw std::invalid_argument("Missing the required adjustment table for female.");
        }
    }

    /// @brief The risk factors adjustment table values
    RiskFactorSexAgeTable values{};
};

/// @brief Defines the baseline risk factors adjustment model.
class RiskfactorAdjustmentModel {
  public:
    RiskfactorAdjustmentModel() = delete;
    /// @brief Initialises a new instance of the BaselineAdjustment class
    /// @param adjustments The baseline adjustment definition
    RiskfactorAdjustmentModel(BaselineAdjustment &adjustments);

    /// @brief Applies the baseline adjustments to the population risk factor values
    /// @param context The simulation shared runtime context instance
    /// @throws std::runtime_error for simulation instances out of synchronisation or message
    /// timeout.
    void Apply(RuntimeContext &context);

  private:
    RiskFactorSexAgeTable get_adjustment_coefficients(RuntimeContext &context) const;

    RiskFactorSexAgeTable calculate_adjustment_coefficients(RuntimeContext &context) const;

    static RiskFactorSexAgeTable calculate_simulated_mean(RuntimeContext &context);

    std::reference_wrapper<BaselineAdjustment> adjustments_;
};
} // namespace hgps
