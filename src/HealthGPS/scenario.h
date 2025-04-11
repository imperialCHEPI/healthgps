#pragma once
#include "HealthGPS.Core/identifier.h"
#include "channel.h"
#include "person.h"
#include "random_algorithm.h"
#include "sync_message.h"

#include <memory>
#include <string>

namespace hgps {

/// @brief Scenario data synchronisation channel type
using SyncChannel = Channel<std::unique_ptr<SyncMessage>>;

/// @brief Health GPS policy scenario types enumeration
enum class ScenarioType : uint8_t {
    /// @brief Baseline scenario
    baseline,

    /// @brief Intervention scenario
    intervention,
};

/// @brief Health-GPS simulation scenario interface
/// @details The scenarios communication is one way from
/// \c baseline to \c intervention, the ScenarioType is
/// by the Health-GPS simulation engine to identify the
/// data-flow direction and send or receive messages.
class Scenario {
  public:
    /// @brief Destroys a Scenario instance
    virtual ~Scenario() = default;

    /// @brief Gets the scenario type identifier
    /// @return The scenario type
    virtual ScenarioType type() const noexcept = 0;

    /// @brief Gets the scenario type name
    /// @return Scenario type name
    virtual std::string name() = 0;

    /// @brief Gets the Scenario communication channel
    /// @return The communication channel
    virtual SyncChannel &channel() = 0;

    /// @brief Clear the scenario log book data
    virtual void clear() noexcept = 0;

    /// @brief Applies this Scenario to the Person instance
    /// @param generator Random number generator instance
    /// @param entity Person instance to apply
    /// @param time Current simulation time
    /// @param risk_factor_key Target risk factor identifier
    /// @param value Current risk factor value
    /// @return The risk factor new value
    virtual double apply(Random &generator, Person &entity, int time,
                         const core::Identifier &risk_factor_key, double value) = 0;
};
} // namespace hgps
