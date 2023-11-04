#pragma once

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include "map2d.h"
#include "risk_factor_model.h"
#include "runtime_context.h"

// TODO: replace BaselineAdjustment with one of these (BaselineAdjustment is a thin wrapper to one
// of these with error checking -- which can be done in model_parser instead)
#include "riskfactor_adjustment.h" // TODO: use new table class

namespace hgps {

/// @brief Defines a table type for double values by sex and age
using SexAgeTable = hgps::Map2D<hgps::core::Gender, hgps::core::Identifier, std::vector<double>>;

/// @brief Defines the risk factor baseline adjustment data type
struct RiskFactorSexAgeTable final {
    /// @brief Initialises a new instance of the RiskFactorSexAgeTable structure
    RiskFactorSexAgeTable() = default;

    /// @brief Initialises a new instance of the RiskFactorSexAgeTable structure
    /// @param adjustment_table The baseline adjustment table
    /// @throws HgpsException for empty adjustment table or table missing ga gender entry
    RiskFactorSexAgeTable(SexAgeTable &&adjustment_table) : values{std::move(adjustment_table)} {

        if (values.empty()) {
            throw core::HgpsException("The risk factors adjustment table must not be empty.");
        } else if (values.rows() != 2) {
            throw core::HgpsException(
                "The risk factors adjustment definition must contain male and female tables.");
        }

        if (!values.contains(core::Gender::male)) {
            throw core::HgpsException("Missing the required adjustment table for male.");
        } else if (!values.contains(core::Gender::female)) {
            throw core::HgpsException("Missing the required adjustment table for female.");
        }
    }

    /// @brief The risk factors adjustment table values
    SexAgeTable values{};
};

/// @brief Risk factor model interface with mean adjustment by sex and age
class RiskFactorAdjustableModel : public RiskFactorModel {
  public:
    /// @brief Constructs a new RiskFactorAdjustableModel instance
    /// @param risk_factor_expected The expected risk factor values by sex and age
    RiskFactorAdjustableModel(const SexAgeTable &risk_factor_expected);

    /// @brief Gets the expected risk factor values by sex and age
    /// @returns The expected risk factor values by sex and age
    const SexAgeTable &get_risk_factor_expected() const noexcept;

    /// @brief Adjust ALL risk factors such that mean simulated value matches expected value
    /// @param context The simulation run-time context
    virtual void adjust_risk_factors(RuntimeContext &context) const;

    /// @brief Adjust SOME risk factors such that mean simulated value matches expected value
    /// @param context The simulation run-time context
    /// @param names The list of risk factors to be adjusted
    virtual void adjust_risk_factors(RuntimeContext &context,
                                     const std::vector<core::Identifier> &names) const;

  private:
    SexAgeTable calculate_simulated_mean(RuntimeContext &context) const;

    SexAgeTable calculate_adjustments(RuntimeContext &context) const;

    SexAgeTable get_adjustments(RuntimeContext &context) const;

    const SexAgeTable &risk_factor_expected_;
};

} // namespace hgps
