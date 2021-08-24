#pragma once

#include <string>
#include <format>

namespace hgps {

    enum class EventType { run_marker, info, result, error };

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
