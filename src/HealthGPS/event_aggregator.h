#pragma once
#include <functional>
#include <stdexcept>

#include "event_message.h"
#include <memory>

namespace hgps {

/// @brief Defines the event handler identifier type
struct EventHandlerIdentifier {
    EventHandlerIdentifier() = delete;

    /// @brief Initialises a new instance of the EventHandlerIdentifier structure
    /// @param identifier The event unique identifier
    /// @throws std::invalid_argument for empty event handler identifier
    EventHandlerIdentifier(std::string identifier) : identifier_{identifier} {
        if (identifier.empty()) {
            throw std::invalid_argument("Event handler identifier must not be empty.");
        }
    }

    /// @brief Gets the event handle identifier string representation
    /// @return The identifier string representation
    const std::string str() const noexcept { return identifier_; }

    /// @brief Compare this instance with other EventHandlerIdentifier instance
    /// @param other The other instance to compare
    /// @return The comparison operation result
    auto operator<=>(const EventHandlerIdentifier &other) const = default;

  private:
    std::string identifier_;
};

/// @brief Defines the event subscriber interface type
class EventSubscriber {
  public:
    /// @brief Initialises a new instance of the EventSubscriber class
    EventSubscriber() = default;

    EventSubscriber(const EventSubscriber &) = delete;
    EventSubscriber(EventSubscriber &&) = delete;
    EventSubscriber &operator=(const EventSubscriber &) = delete;
    EventSubscriber &operator=(EventSubscriber &&) = delete;

    /// @brief Destroys a EventSubscriber instance
    virtual ~EventSubscriber() = default;

    /// @brief Unsubscribes from the event
    virtual void unsubscribe() const = 0;

    /// @brief Gets the subscriber unique identifier
    /// @return The event handler identifier
    [[nodiscard]] virtual EventHandlerIdentifier id() const noexcept = 0;
};

/// @brief Defines the event aggregator interface type
class EventAggregator {
  public:
    /// @brief Initialises a new instance of the EventAggregator class
    EventAggregator() = default;

    /// @brief Constructs the EventAggregator using move semantics.
    /// @param other The other EventAggregator instance to move
    EventAggregator(EventAggregator &&other) = default;

    /// @brief Replaces the EventAggregator contents with the other using move semantics
    /// @param other The other EventAggregator instance to move
    /// @return This instance
    EventAggregator &operator=(EventAggregator &&other) = default;

    EventAggregator(const EventAggregator &) = delete;
    EventAggregator &operator=(const EventAggregator &) = delete;

    /// @brief Destroys a EventAggregator instance
    virtual ~EventAggregator() = default;

    /// @brief Subscribes a handler function to an event type identifier
    /// @param event_id The event type to subscribe
    /// @param handler The event handler function to register
    /// @return The event subscriber instance
    [[nodiscard]] virtual std::unique_ptr<EventSubscriber>
    subscribe(EventType event_id,
              std::function<void(std::shared_ptr<EventMessage> message)> handler) = 0;

    /// @brief Unsubscribes from an event notification
    /// @param subscriber The event subscriber instance
    /// @return true if the operation succeeded; otherwise, false
    virtual bool unsubscribe(const EventSubscriber &subscriber) = 0;

    /// @brief Publishes a message to all subscribers synchronous
    /// @param message The new message instance
    virtual void publish(std::unique_ptr<EventMessage> message) = 0;

    /// @brief Publishes a message to all subscribers asynchronous
    /// @param message The new message instance
    virtual void publish_async(std::unique_ptr<EventMessage> message) = 0;
};

} // namespace hgps
