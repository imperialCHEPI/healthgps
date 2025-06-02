#pragma once

#include "HealthGPS.Core/datastore.h"
#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/poco.h"

#include "disease_definition.h"
#include "interfaces.h"
#include "lms_definition.h"
#include "modelinput.h"
#include "risk_factor_adjustable_model.h"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace hgps {

/// @brief Defines the linear model parameters data type
struct LinearModelParams {
    double intercept;
    std::unordered_map<std::string, double> coefficients;
};

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

    /// @brief Gets the systolic blood pressure models
    /// @return The systolic blood pressure models
    virtual std::unordered_map<core::Identifier, LinearModelParams> get_systolic_blood_pressure_models() const = 0;
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

    void clear_cache() noexcept;

    std::unordered_map<core::Identifier, LinearModelParams> get_systolic_blood_pressure_models() const override;

  private:
    mutable std::mutex mutex_;
    std::reference_wrapper<core::Datastore> data_manager_;
    std::map<RiskFactorModelType, std::unique_ptr<RiskFactorModelDefinition>> rf_model_definition_;
    std::vector<core::DiseaseInfo> diseases_info_;
    std::map<core::Identifier, DiseaseDefinition> diseases_;
    LmsDefinition lms_parameters_;

    void load_disease_definition(const core::DiseaseInfo &info, const ModelInput &config);
};
} // namespace hgps
