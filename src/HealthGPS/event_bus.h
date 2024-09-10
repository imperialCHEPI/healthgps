#pragma once

#include "event_subscriber.h"
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace hgps {

/// @brief Implements the default memory-based simulation event bus type
class DefaultEventBus final : public EventAggregator {
  public:
    [[nodiscard]] std::unique_ptr<EventSubscriber>
    subscribe(EventType event_id,
              std::function<void(std::shared_ptr<EventMessage> message)> function) override;

    void publish(std::unique_ptr<EventMessage> message) const override;

    void publish_async(std::unique_ptr<EventMessage> message) const override;

    bool unsubscribe(const EventSubscriber &subscriber) override;

    /// @brief Gets the number of registered subscribers
    /// @return Number of subscribers
    [[nodiscard]] std::size_t count();

    /// @brief Clear all registered subscribers
    void clear() noexcept;

  private:
    using mutex_type = std::shared_mutex;
    mutable mutex_type subscribe_mutex_;
    std::unordered_multimap<int, std::string> registry_;
    std::unordered_map<std::string, std::function<void(std::shared_ptr<EventMessage>)>>
        subscribers_;

    template <typename Callable> void shared_access(Callable &&callable) const {
        try {
            std::shared_lock<mutex_type> lock(subscribe_mutex_);
            callable();
        } catch (std::system_error &) {
            // do nothing
        }
    }

    template <typename Callable> void exclusive_access(Callable &&callable) const {
        try {
            std::unique_lock<mutex_type> lock(subscribe_mutex_);
            callable();
        } catch (std::system_error &) {
            // do nothing
        }
    }
};
} // namespace hgps
