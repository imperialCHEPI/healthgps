#pragma once
#include "event_message.h"

namespace hgps {

/// @brief Enumerates the simulation executive actions
enum class RunnerAction {
    /// @brief Start simulation experiment
    start,

    /// @brief Begin a new simulation run
    run_begin,

    /// @brief Simulation experiment has been cancelled
    cancelled,

    /// @brief End a simulation run
    run_end,

    /// @brief Finish simulation experiment
    finish
};

/// @brief Implements the simulation executive event message data type
struct RunnerEventMessage final : public EventMessage {

    RunnerEventMessage() = delete;

    /// @brief Initialise a new instance of the RunnerEventMessage class.
    /// @param sender The sender identifier
    /// @param run_action The event action
    RunnerEventMessage(std::string sender, RunnerAction run_action) noexcept;

    /// @brief Initialise a new instance of the RunnerEventMessage class.
    /// @param sender The sender identifier
    /// @param run_action The event action
    /// @param elapsed Action elapsed time in milliseconds
    RunnerEventMessage(std::string sender, RunnerAction run_action, double elapsed) noexcept;

    /// @brief Initialise a new instance of the RunnerEventMessage class.
    /// @param sender The sender identifier
    /// @param run_action The event action
    /// @param run The simulation run number
    RunnerEventMessage(std::string sender, RunnerAction run_action, unsigned int run) noexcept;

    /// @brief Initialise a new instance of the RunnerEventMessage class.
    /// @param sender The sender identifier
    /// @param run_action The event action
    /// @param run The simulation run number
    /// @param elapsed Action elapsed time in milliseconds
    RunnerEventMessage(std::string sender, RunnerAction run_action, unsigned int run,
                       double elapsed) noexcept;

    /// @brief The simulation executive action
    RunnerAction action{};

    /// @brief The action elapsed time in milliseconds
    double elapsed_ms{};

    int id() const noexcept override;

    std::string to_string() const override;

    void accept(EventMessageVisitor &visitor) const override;
};
} // namespace hgps
