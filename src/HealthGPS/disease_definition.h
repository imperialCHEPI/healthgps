#pragma once

#include "HealthGPS.Core/poco.h"
#include "disease_table.h"
#include "gender_value.h"
#include "relative_risk.h"

namespace hgps {

/// @brief Disease definition parameter value lookup by age and gender
using ParameterLookup = const std::map<int, DoubleGenderValue>;

/// @brief Defines the cancer disease parameters data type
struct DiseaseParameter final {
    /// @brief Initialises a new instance of the DiseaseParameter data structure
    DiseaseParameter() = default;

    /// @brief Initialises a new instance of the DiseaseParameter data structure
    /// @param data_time The disease data time reference
    /// @param prevalence The cancer prevalence distribution
    /// @param survival The cancer survivalist rates
    /// @param deaths The cancer death weights
    DiseaseParameter(int data_time, const ParameterLookup &prevalence,
                     const ParameterLookup &survival, const ParameterLookup &deaths)
        : time_year{data_time}, prevalence_distribution{prevalence}, survival_rate{survival},
          death_weight{deaths}, max_time_since_onset{prevalence.rbegin()->first + 1} {}

    /// @brief The dataset time reference
    int time_year{};

    /// @brief Cancer prevalence distribution
    ParameterLookup prevalence_distribution{};

    /// @brief Cancer survival rate
    ParameterLookup survival_rate{};

    /// @brief Cancer death weight
    ParameterLookup death_weight{};

    /// @brief The maximum time since onset for the cancer to be cured.
    int max_time_since_onset{};
};

/// @brief Implements the disease full definition data type.
class DiseaseDefinition final {
  public:
    /// @brief Initialises a new instance of the DiseaseDefinition class for standard diseases
    /// @param measures_table The disease measures table
    /// @param diseases The diseases to disease relative risk table
    /// @param risk_factors The risk-factors to diseases relative risk values
    DiseaseDefinition(DiseaseTable &&measures_table, RelativeRiskTableMap &&diseases,RelativeRiskLookupMap &&risk_factors)
        : measures_table_{std::move(measures_table)}, relative_risk_diseases_{std::move(diseases)},
          relative_risk_factors_{std::move(risk_factors)}, parameters_{} {}

    /// @brief Initialises a new instance of the DiseaseDefinition class for cancer diseases
    /// @param measures_table The disease measures table
    /// @param diseases The diseases to disease relative risk table
    /// @param risk_factors The risk-factors to diseases relative risk values
    /// @param parameter The cancer disease parameter
    DiseaseDefinition(DiseaseTable &&measures_table, RelativeRiskTableMap &&diseases,
                      RelativeRiskLookupMap &&risk_factors, DiseaseParameter &&parameter)
        : measures_table_{std::move(measures_table)}, relative_risk_diseases_{std::move(diseases)},
          relative_risk_factors_{std::move(risk_factors)}, parameters_{std::move(parameter)} {}

    /// @brief Gets the disease unique identifier
    /// @return The disease identifier
    const core::DiseaseInfo &identifier() const noexcept { return measures_table_.info(); }

    /// @brief Gets the disease measures table, e.g., prevalence, incidence, etc., rates
    /// @return The disease measures table
    const DiseaseTable &table() const noexcept { return measures_table_; }

    /// @brief Gets the diseases relative risk to disease table
    /// @return The disease-disease relative risk table
    const RelativeRiskTableMap &relative_risk_diseases() const noexcept {
        return relative_risk_diseases_;
    }

    /// @brief Gets the risk factors relative risk to disease table
    /// @return The risk factors to disease relative risk values
    const RelativeRiskLookupMap &relative_risk_factors() const noexcept {
        return relative_risk_factors_;
    }

    /// @brief Gets the cancer disease parameters
    /// @return The cancer parameters
    const DiseaseParameter &parameters() const noexcept { return parameters_; }

  private:
    DiseaseTable measures_table_;
    RelativeRiskTableMap relative_risk_diseases_;
    RelativeRiskLookupMap relative_risk_factors_;
    DiseaseParameter parameters_;
};
} // namespace hgps
