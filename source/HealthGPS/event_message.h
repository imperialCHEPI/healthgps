#pragma once

#include <string>

namespace hgps {

    enum class EventType { runner, info, result, error };

    struct EventMessage
    {
        EventMessage() = default;
        EventMessage(const EventMessage&) = delete;
        EventMessage(EventMessage&&) = delete;
        EventMessage& operator=(const EventMessage&) = delete;
        EventMessage& operator=(EventMessage&&) = delete;
        virtual ~EventMessage() = default;

        virtual int id() const noexcept = 0;

        virtual std::string to_string() const = 0;
    };
}
