#include "event_bus.h"
#include "HealthGPS.Core/thread_util.h"
#include <crossguid/guid.hpp>

namespace hgps {

std::unique_ptr<EventSubscriber>
DefaultEventBus::subscribe(EventType event_id,
                           std::function<void(std::shared_ptr<EventMessage> message)> function) {
    auto handle_id = std::string{};
    exclusive_access([&]() {
        auto event_key = static_cast<int>(event_id);
        handle_id = xg::newGuid().str();
        subscribers_.emplace(handle_id, std::move(function));
        registry_.emplace(event_key, handle_id);
    });

    return std::make_unique<EventSubscriberHandler>(handle_id, this);
}

void DefaultEventBus::publish(std::unique_ptr<EventMessage> message) const {
    shared_access([&]() {
        std::shared_ptr<EventMessage> shared_message = std::move(message);

        // Only call the functions we need for the event type
        auto [begin_id, end_id] = registry_.equal_range(shared_message->id());
        for (; begin_id != end_id; ++begin_id) {
            subscribers_.at(begin_id->second)(shared_message);
        }
    });
}

void DefaultEventBus::publish_async(std::unique_ptr<EventMessage> message) const {
    auto futptr = std::make_shared<std::future<void>>();
    *futptr = core::run_async(&DefaultEventBus::publish, this, std::move(message));
}

bool DefaultEventBus::unsubscribe(const EventSubscriber &subscriber) {
    auto result = false;
    exclusive_access([this, &result, &subscriber]() {
        auto sub_id = subscriber.id().str();
        if (subscribers_.contains(sub_id)) {
            for (auto it = registry_.begin(); it != registry_.end(); ++it) {
                if (it->second == subscriber.id()) {
                    registry_.erase(it);
                    result = true;
                    break;
                }
            }

            if (result) {
                subscribers_.erase(sub_id);
            }
        }
    });

    return result;
}

std::size_t DefaultEventBus::count() {
    std::shared_lock<mutex_type> lock(subscribe_mutex_);
    std::size_t count{};
    shared_access([this, &count]() noexcept { count = registry_.size(); });
    return count;
}

void DefaultEventBus::clear() noexcept {
    exclusive_access([this]() noexcept {
        registry_.clear();
        subscribers_.clear();
    });
}
} // namespace hgps
