#pragma once

#include <string>

namespace hgps {

    enum class EventType { runner, info, result, error };

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
    };
}
