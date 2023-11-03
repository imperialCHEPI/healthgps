#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "runtime_context.h"

namespace hgps {

/// @brief Health GPS risk factor module types enumeration
enum class RiskFactorModelType : uint8_t {
    /// @brief Static model
    Static,

    /// @brief Dynamic model
    Dynamic,
};

/// @brief Risk factor model interface
class RiskFactorModel {
  public:
    /// @brief Destroys a RiskFactorModel instance
    virtual ~RiskFactorModel() = default;

    /// @brief Gets the model type identifier
    /// @return The module type identifier
    virtual RiskFactorModelType type() const noexcept = 0;

    /// @brief Gets the model name
    /// @return The human-readable model name
    virtual std::string name() const noexcept = 0;

    /// @brief Generates the initial risk factors for a population and newborns
    /// @param context The simulation run-time context
    virtual void generate_risk_factors(RuntimeContext &context) = 0;

    /// @brief Update risk factors for population
    /// @param context The simulation run-time context
    virtual void update_risk_factors(RuntimeContext &context) = 0;
};

/// @brief Risk factor model definition interface
class RiskFactorModelDefinition {
  public:
    /// @brief Destroys a RiskFactorModelDefinition instance
    virtual ~RiskFactorModelDefinition() = default;

    /// @brief Creates a new risk factor model from this definition
    virtual std::unique_ptr<RiskFactorModel> create_model() const = 0;
};

} // namespace hgps
