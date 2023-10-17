#pragma once
#include "disease_definition.h"
#include "energy_balance_hierarchical_model.h"
#include "energy_balance_model.h"
#include "interfaces.h"
#include "lms_definition.h"
#include "modelinput.h"
#include "riskfactor_adjustment_types.h"
#include "static_hierarchical_linear_model.h"
#include "static_linear_model.h"
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

    /// @brief Gets the user provided baseline risk factors adjustment dataset
    /// @return Baseline risk factors adjustments
    virtual BaselineAdjustment &get_baseline_adjustment_definition() = 0;

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

    /// @brief Register a user provided baseline risk factors adjustments dataset
    /// @param definition The baseline risk factors adjustments dataset
    void register_baseline_adjustment_definition(BaselineAdjustment definition);

    core::Datastore &manager() noexcept override;

    const RiskFactorModelDefinition &
    get_risk_factor_model_definition(const RiskFactorModelType &model_type) const override;

    BaselineAdjustment &get_baseline_adjustment_definition() override;

    const std::vector<core::DiseaseInfo> &get_diseases() override;

    std::optional<core::DiseaseInfo> get_disease_info(core::Identifier code) override;

    DiseaseDefinition &get_disease_definition(const core::DiseaseInfo &info,
                                              const ModelInput &config) override;

    LmsDefinition &get_lms_definition() override;

    void clear_cache() noexcept;

  private:
    mutable std::mutex mutex_;
    std::reference_wrapper<core::Datastore> data_manager_;
    std::map<RiskFactorModelType, std::unique_ptr<RiskFactorModelDefinition>> rf_model_definition_;
    BaselineAdjustment baseline_adjustments_;
    std::vector<core::DiseaseInfo> diseases_info_;
    std::map<core::Identifier, DiseaseDefinition> diseases_;
    LmsDefinition lms_parameters_;

    void load_disease_definition(const core::DiseaseInfo &info, const ModelInput &config);
};
} // namespace hgps
