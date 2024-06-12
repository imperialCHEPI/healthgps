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
    virtual Country get_country(const std::string &alpha) const = 0;

    /// @brief Gets the population growth trend for a country filtered by time
    /// @param country The target country definition
    /// @param time_filter The time filter predicate, e.g., years range
    /// @return The resulting list of population trend items
    virtual std::vector<PopulationItem>
    get_population(const Country &country, std::function<bool(unsigned int)> time_filter) const = 0;

    /// @brief Gets the population mortality trend for a country filtered by time
    /// @param country The target country definition
    /// @param time_filter The time filter predicate, e.g., years range
    /// @return The resulting list of mortality trend items
    virtual std::vector<MortalityItem>
    get_mortality(const Country &country, std::function<bool(unsigned int)> time_filter) const = 0;

    /// @brief Gets the collection of diseases information in the store
    /// @return The list of diseases defined
    virtual std::vector<DiseaseInfo> get_diseases() const = 0;

    /// @brief Gets a single disease information by identifier
    /// @param code The target disease identifier
    /// @return The disease information
    virtual DiseaseInfo get_disease_info(const Identifier &code) const = 0;

    /// @brief Gets a disease full definition by identifier for a country
    /// @param info The target disease information
    /// @param country The target country definition
    /// @return The disease definition
    virtual DiseaseEntity get_disease(const DiseaseInfo &info, const Country &country) const = 0;

    /// @brief Gets the relative risk effects for disease to disease interactions
    /// @param source The source disease information
    /// @param target The target disease information
    /// @return The disease-disease effects or std::nullopt if file doesn't exist or only contains
    ///         default values
    virtual std::optional<RelativeRiskEntity>
    get_relative_risk_to_disease(const DiseaseInfo &source, const DiseaseInfo &target) const = 0;

    /// @brief Gets the relative risk effects for risk factor to disease interactions
    /// @param source The disease information
    /// @param gender The gender enumeration
    /// @param risk_factor_key The risk factor identifier
    /// @return The risk factor-disease effects or std::nullopt if data missing
    virtual std::optional<RelativeRiskEntity>
    get_relative_risk_to_risk_factor(const DiseaseInfo &source, Gender gender,
                                     const Identifier &risk_factor_key) const = 0;

    /// @brief Gets the parameters required by cancer type diseases for a country
    /// @param info The disease of type cancer information
    /// @param country The target country definition
    /// @return The cancer parameters
    virtual CancerParameterEntity get_disease_parameter(const DiseaseInfo &info,
                                                        const Country &country) const = 0;

    /// @brief Gets the Burden of Diseases (BoD) analysis dataset for a country
    /// @param country The target country definition
    /// @return The BoD analysis data
    virtual DiseaseAnalysisEntity get_disease_analysis(const Country &country) const = 0;

    /// @brief Gets the population birth indicators for a country filtered by time
    /// @param country The target country definition
    /// @param time_filter The time filter predicate, e.g. years range
    /// @return The resulting list of birth indicator items
    virtual std::vector<BirthItem>
    get_birth_indicators(const Country &country,
                         std::function<bool(unsigned int)> time_filter) const = 0;

    /// @brief Gets the LMS (lambda-mu-sigma) parameters for childhood growth charts
    /// @return The parameters for the LMS model
    virtual std::vector<LmsDataRow> get_lms_parameters() const = 0;
};

} // namespace hgps::core
