#pragma once

#include <string>
#include <cstdint>
#include "event_visitor.h"

namespace hgps {

    /// @brief Event Message type enumeration  
    enum struct EventType : uint8_t 
    {
        /// @brief Simulation executive message
        runner,

        /// @brief General notification and progress message 
        info,

        /// @brief Simulation result message
        result,

        /// @brief General error reporting message
        error
    };

    /// @brief Event message interface
    struct EventMessage
    {
        EventMessage() = delete;
        EventMessage(std::string sender, unsigned int run) 
            : source{ sender }, run_number{ run }{}

        EventMessage(const EventMessage&) = delete;
        EventMessage(EventMessage&&) = delete;
        EventMessage& operator=(const EventMessage&) = delete;
        EventMessage& operator=(EventMessage&&) = delete;
        virtual ~EventMessage() = default;

        const std::string source;

        const unsigned int run_number{};

        virtual int id() const noexcept = 0;

        virtual std::string to_string() const = 0;

        virtual void accept(EventMessageVisitor& visitor) const = 0;
    };
}
