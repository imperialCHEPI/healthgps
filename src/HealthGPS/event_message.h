#pragma once

#include "event_visitor.h"
#include <cstdint>
#include <string>

namespace hgps {

/// @brief Event Message type enumeration
enum struct EventType : uint8_t {
    /// @brief Simulation executive message
    runner,

    /// @brief General notification and progress message
    info,

    /// @brief Simulation result message
    result,

    /// @brief General error reporting message
    error
};

/// @brief Simulation event messages interface
struct EventMessage {
    EventMessage() = delete;

    /// @brief Initialises a new instance of the EventMessage structure.
    /// @param sender The sender identifier
    /// @param run Current simulation run number
    EventMessage(std::string sender, unsigned int run)
        : source{std::move(sender)}, run_number{run} {}

    EventMessage(const EventMessage &) = delete;
    EventMessage(EventMessage &&) = delete;
    EventMessage &operator=(const EventMessage &) = delete;
    EventMessage &operator=(EventMessage &&) = delete;

    /// @brief Destroys an EventMessage instance
    virtual ~EventMessage() = default;

    /// @brief Gets the sender identifier
    const std::string source;

    /// @brief Gets the associated Simulation run number
    const unsigned int run_number{};

    /// @brief Gets the unique message type identifier
    /// @return The message type identifier
    virtual int id() const noexcept = 0;

    /// @brief Create a string representation of this instance
    /// @return The string representation
    virtual std::string to_string() const = 0;

    /// @brief Double dispatch the message using a visitor implementation
    /// @param visitor The event message instance to accept
    virtual void accept(EventMessageVisitor &visitor) const = 0;
};
} // namespace hgps
