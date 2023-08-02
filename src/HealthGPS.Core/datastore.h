#pragma once

#include "interval.h"
#include "poco.h"
#include <optional>
#include <vector>

namespace hgps::core {

/// @brief Defines the `Health-GPS` back-end data store interface for all implementations.
///
/// @sa https://imperialchepi.github.io/healthgps/datamodel for the `Data Model` details.
class Datastore {
  public:
    /// @brief Destroys a Datastore instance
    virtual ~Datastore() = default;

    /// @brief Gets the full collection of countries in the store
    /// @return The list of countries
    virtual std::vector<Country> get_countries() const = 0;

    /// @brief Gets a single country by the alpha code
    /// @param alpha The country alpha 2 or 3 format code to search
    /// @return The country's definition
    virtual Country get_country(std::string alpha) const = 0;

    /// @brief Gets the population growth trend for a country filtered by time
    /// @param country The target country definition
    /// @param time_filter The time filter predicate, e.g., years range
    /// @return The resulting list of population trend items
    virtual std::vector<PopulationItem>
    get_population(Country country,
                   const std::function<bool(const unsigned int &)> time_filter) const = 0;

    /// @brief Gets the population mortality trend for a country filtered by time
    /// @param country The target country definition
    /// @param time_filter The time filter predicate, e.g., years range
    /// @return The resulting list of mortality trend items
    virtual std::vector<MortalityItem>
    get_mortality(Country country,
                  const std::function<bool(const unsigned int &)> time_filter) const = 0;

    /// @brief Gets the collection of diseases information in the store
    /// @return The list of diseases defined
    virtual std::vector<DiseaseInfo> get_diseases() const = 0;

    /// @brief Gets a single disease information by identifier
    /// @param code The target disease identifier
    /// @return The disease information, if found, otherwise empty
    virtual std::optional<DiseaseInfo> get_disease_info(Identifier code) const = 0;

    /// @brief Gets a disease full definition by identifier for a country
    /// @param info The target disease information
    /// @param country The target country definition
    /// @return The disease definition, check empty() = true for missing data.
    virtual DiseaseEntity get_disease(DiseaseInfo info, Country country) const = 0;

    /// @brief Gets the relative risk effects for disease to disease interactions
    /// @param source The source disease information
    /// @param target The target disease information
    /// @return The disease-disease effects, check empty() = true for missing data.
    virtual RelativeRiskEntity get_relative_risk_to_disease(DiseaseInfo source,
                                                            DiseaseInfo target) const = 0;

    /// @brief Gets the relative risk effects for risk factor to disease interactions
    /// @param source The disease information
    /// @param gender The gender enumeration
    /// @param risk_factor_key The risk factor identifier
    /// @return The risk factor-disease effects, check empty() = true for missing data.
    virtual RelativeRiskEntity
    get_relative_risk_to_risk_factor(DiseaseInfo source, Gender gender,
                                     Identifier risk_factor_key) const = 0;

    /// @brief Gets the parameters required by cancer type diseases for a country
    /// @param info The disease of type cancer information
    /// @param country The target country definition
    /// @return The cancer parameters, check empty() = true for missing data.
    virtual CancerParameterEntity get_disease_parameter(DiseaseInfo info,
                                                        Country country) const = 0;

    /// @brief Gets the Burden of Diseases (BoD) analysis dataset for a country
    /// @param country The target country definition
    /// @return The BoD analysis data, check empty() = true for missing data.
    virtual DiseaseAnalysisEntity get_disease_analysis(const Country country) const = 0;

    /// @brief Gets the population birth indicators for a country filtered by time
    /// @param country The target country definition
    /// @param time_filter The time filter predicate, e.g. years range
    /// @return The resulting list of birth indicator items
    virtual std::vector<BirthItem>
    get_birth_indicators(const Country country,
                         const std::function<bool(const unsigned int &)> time_filter) const = 0;

    /// @brief Gets the LMS (lambda-mu-sigma) parameters for childhood growth charts
    /// @return The parameters for the LMS model
    virtual std::vector<LmsDataRow> get_lms_parameters() const = 0;
};
} // namespace hgps::core