#pragma once
#include "event_message.h"

namespace hgps {

/// @brief Enumerates simulation model actions
enum class ModelAction {
    /// @brief Simulation has started, time = start
    start,

    /// @brief Simulation time has updated, time = time + 1
    update,

    /// @brief Simulation has stopped, time = end
    stop
};

/// @brief Implements the simulation information event message data type
struct InfoEventMessage final : public EventMessage {

    InfoEventMessage() = delete;

    /// @brief Initialises a new instance of the ErrorEventMessage structure.
    /// @param sender The sender identifier
    /// @param action Source action identification
    /// @param run Current simulation run number
    /// @param time Current simulation time
    InfoEventMessage(std::string sender, ModelAction action, unsigned int run, int time) noexcept;

    /// @brief Initialises a new instance of the ErrorEventMessage structure.
    /// @param sender The sender identifier
    /// @param action Source action identification
    /// @param run Current simulation run number
    /// @param time Current simulation time
    /// @param msg The notification message
    InfoEventMessage(std::string sender, ModelAction action, unsigned int run, int time,
                     std::string msg) noexcept;

    /// @brief Gets the source action value
    const ModelAction model_action{};

    /// @brief Gets the associated Simulation time
    const int model_time{};

    /// @brief Gets the notification message
    const std::string message;

    int id() const noexcept override;

    std::string to_string() const override;

    void accept(EventMessageVisitor &visitor) const override;
};

namespace detail {
/// @brief Converts enumeration to string, not pretty but no support in C++
std::string model_action_str(const ModelAction action);
} // namespace detail
} // namespace hgps
