#pragma once

#include "modelinput.h"
#include "repository.h"

namespace hgps {

/// @brief Defines the risk factors module container to hold risk factor models.
class RiskFactorModule final : public RiskFactorHostModule {
  public:
    RiskFactorModule() = delete;

    /// @brief Initialises a new instance of the RiskFactorModule class.
    /// @param models Collection of model instances
    /// @throws std::invalid_argument for empty models collection or missing module type.
    /// @throws std::out_of_range for model type and model instance type mismatch.
    RiskFactorModule(std::map<RiskFactorModelType, std::unique_ptr<RiskFactorModel>> models);

    SimulationModuleType type() const noexcept override;

    std::string name() const noexcept override;

    std::size_t size() const noexcept override;

    bool contains(const RiskFactorModelType &model_type) const noexcept override;

    RiskFactorModel &at(const RiskFactorModelType &model_type) const;

    void initialise_population(RuntimeContext &context);

    void update_population(RuntimeContext &context);

    void initialise_population(RuntimeContext &context, Population &population, Random &random) override;

  private:
    std::map<RiskFactorModelType, std::unique_ptr<RiskFactorModel>> models_;
    std::string name_{"RiskFactor"};
};

std::unique_ptr<RiskFactorModule> build_risk_factor_module(Repository &repository,
                                                           const ModelInput &config);

} // namespace hgps
