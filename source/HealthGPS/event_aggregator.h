#pragma once
#include <functional>
#include "event_message.h"

namespace hgps {

    struct EventHandlerIdentifier {
        EventHandlerIdentifier() = delete;
        EventHandlerIdentifier(std::string identifier)
            : identifier_{ identifier } {
            if (identifier.empty()) {
                throw std::invalid_argument("Event handler identifier must not be empty.");
            }
        }

        const std::string str() const noexcept { return identifier_; }

        auto operator<=>(const EventHandlerIdentifier&) const = default;

    private:
        std::string identifier_;
    };

    class EventSubscriber {
    public:
        EventSubscriber() = default;
        EventSubscriber(const EventSubscriber&) = delete;
        EventSubscriber(EventSubscriber&&) = delete;
        EventSubscriber& operator=(const EventSubscriber&) = delete;
        EventSubscriber& operator=(EventSubscriber&&) = delete;
        virtual ~EventSubscriber() = default;

        virtual void unregister() const = 0;

        [[nodiscard]]
        virtual const EventHandlerIdentifier id() const noexcept = 0;
    };

    class EventAggregator {
    public:
        EventAggregator() = default;
        EventAggregator(EventAggregator&&) = default;
        EventAggregator& operator=(EventAggregator&&) = default;

        EventAggregator(const EventAggregator&) = delete;
        EventAggregator& operator=(const EventAggregator&) = delete;
        virtual ~EventAggregator() = default;

        [[nodiscard]]
        virtual std::unique_ptr<EventSubscriber> subscribe(EventType event_id,
            std::function<void(const EventMessage& message)>&& handler) = 0;

        virtual bool unsubscribe(const EventSubscriber& subscriber) = 0;

        virtual void publish(const EventMessage& message) = 0;

        virtual void publish_async(const EventMessage& message) = 0;
    };
}
