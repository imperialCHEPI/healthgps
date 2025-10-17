#pragma once

#include "HealthGPS.Core/datastore.h"

#include "disease_definition.h"
#include "interfaces.h"
#include "lms_definition.h"
#include "modelinput.h"
#include "risk_factor_adjustable_model.h"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace hgps {

/// @brief Define the data repository interface for input datasets and back-end storage
class Repository {
  public:
    /// @brief Initialises a new instance of the Repository class.
    Repository() = default;
    Repository(Repository &&) = delete;
    Repository(const Repository &) = delete;
    Repository &operator=(Repository &&) = delete;
    Repository &operator=(const Repository &) = delete;

    /// @brief Destroys a Repository instance
    virtual ~Repository() = default;

    /// @brief Gets a reference to the back-end storage instance
    /// @return The back-end data storage
    virtual core::Datastore &manager() noexcept = 0;

    /// @brief Gets a user-provided risk factor model definition
    /// @param model_type Static or Dynamic
    /// @return The risk factor model definition
    virtual const RiskFactorModelDefinition &
    get_risk_factor_model_definition(const RiskFactorModelType &model_type) const = 0;

    /// @brief Gets the collection of all diseases available in the back-end storage
    /// @return Collection of diseases information
    virtual const std::vector<core::DiseaseInfo> &get_diseases() = 0;

    /// @brief Gets a disease information by identifier
    /// @param code The disease identifier
    /// @return Disease information, if found; otherwise, empty
    virtual std::optional<core::DiseaseInfo> get_disease_info(core::Identifier code) = 0;

    /// @brief Gets a disease complete definition
    /// @param info The disease information
    /// @param config The user inputs instance
    /// @return The disease definition
    /// @throws std::runtime_error for failure to load disease definition.
    virtual DiseaseDefinition &get_disease_definition(const core::DiseaseInfo &info,
                                                      const ModelInput &config) = 0;

    /// @brief Gets the LMS (lambda-mu-sigma) definition
    /// @return The LMS definition
    virtual LmsDefinition &get_lms_definition() = 0;

    /// @brief Register region prevalence data
    /// @param region_data Map of age-specific region probabilities by gender
    virtual void register_region_prevalence(
        const std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>>
            &region_data) = 0;

    /// @brief Register ethnicity prevalence data
    /// @param ethnicity_data Map of age group, gender, and region-specific ethnicity probabilities
    virtual void register_ethnicity_prevalence(
        const std::map<core::Identifier,
                       std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>>
            &ethnicity_data) = 0;

    /// @brief Gets region prevalence data
    /// @return Map of age-specific region probabilities by gender
    virtual const std::map<core::Identifier,
                           std::map<core::Gender, std::map<std::string, double>>> &
    get_region_prevalence() const = 0;

    /// @brief Gets ethnicity prevalence data
    /// @return Map of age group, gender, and region-specific ethnicity probabilities
    virtual const std::map<
        core::Identifier,
        std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>> &
    get_ethnicity_prevalence() const = 0;
};

/// @brief Implements the cached data repository for input datasets and back-end storage
///
/// @details This repository caches the read-only dataset used by multiple instances
/// of the simulation to minimise back-end data access. This type is thread-safe
class CachedRepository final : public Repository {
  public:
    CachedRepository() = delete;

    /// @brief Initialises a new instance of the CachedRepository class.
    /// @param manager Back-end storage instance
    CachedRepository(core::Datastore &manager);

    /// @brief Register a user provided risk factor model definition
    /// @param model_type Static or Dynamic
    /// @param definition The risk factor model definition instance
    void
    register_risk_factor_model_definition(const RiskFactorModelType &model_type,
                                          std::unique_ptr<RiskFactorModelDefinition> definition);

    core::Datastore &manager() noexcept override;

    const RiskFactorModelDefinition &
    get_risk_factor_model_definition(const RiskFactorModelType &model_type) const override;

    const std::vector<core::DiseaseInfo> &get_diseases() override;

    std::optional<core::DiseaseInfo> get_disease_info(core::Identifier code) override;

    DiseaseDefinition &get_disease_definition(const core::DiseaseInfo &info,
                                              const ModelInput &config) override;

    LmsDefinition &get_lms_definition() override;

    void register_region_prevalence(
        const std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>>
            &region_data) override;

    void register_ethnicity_prevalence(
        const std::map<core::Identifier,
                       std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>>
            &ethnicity_data) override;

    const std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>> &
    get_region_prevalence() const override;

    const std::map<core::Identifier,
                   std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>> &
    get_ethnicity_prevalence() const override;

    void clear_cache() noexcept;

  private:
    mutable std::mutex mutex_;
    std::reference_wrapper<core::Datastore> data_manager_;
    std::map<RiskFactorModelType, std::unique_ptr<RiskFactorModelDefinition>> rf_model_definition_;
    std::vector<core::DiseaseInfo> diseases_info_;
    std::map<core::Identifier, DiseaseDefinition> diseases_;
    LmsDefinition lms_parameters_;

    /// @brief Region assignment probabilities by age and gender
    std::map<core::Identifier, std::map<core::Gender, std::map<std::string, double>>>
        region_prevalence_;

    /// @brief Ethnicity assignment probabilities by age group, gender, and region
    std::map<core::Identifier,
             std::map<core::Gender, std::map<std::string, std::map<std::string, double>>>>
        ethnicity_prevalence_;

    void load_disease_definition(const core::DiseaseInfo &info, const ModelInput &config);
};
} // namespace hgps
