#pragma once

#include "event_subscriber.h"
#include <functional>
#include <shared_mutex>
#include <unordered_map>

namespace hgps {

	class DefaultEventBus final : public EventAggregator
	{
	public:
        [[nodiscard]]
        std::unique_ptr<EventSubscriber> subscribe(EventType event_id,
            std::function<void(const EventMessage& message)>&& function) override;

        void publish(const EventMessage& message) override;

        void publish_async(const EventMessage& message) override;

        bool unsubscribe(const EventSubscriber& subscriber);

        [[nodiscard]] std::size_t count();

        void clear() noexcept;

	private:
        using mutex_type = std::shared_mutex;
        mutable mutex_type subscribe_mutex_;
        std::unordered_multimap<int, std::string> registry_;
        std::unordered_map<std::string, std::function<void(const EventMessage& message)>> subscribers_;

        template<typename Callable>
        void shared_access(Callable&& callable) {
            try {
                std::shared_lock<mutex_type> lock(subscribe_mutex_);
                callable();
            }
            catch (std::system_error&) {
                // do nothing
            }
        }

        template<typename Callable>
        void exclusive_access(Callable&& callable) {
            try {
                std::unique_lock<mutex_type> lock(subscribe_mutex_);
                callable();
            }
            catch (std::system_error&) {
                // do nothing
            }
        }
	};
}