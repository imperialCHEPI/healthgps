#pragma once
#include "event_aggregator.h"

namespace hgps {

	class EventSubscriberHandler final: public EventSubscriber {
    public:
        EventSubscriberHandler() = delete;
        EventSubscriberHandler(EventHandlerIdentifier id, EventAggregator* hub);
        EventSubscriberHandler(const EventSubscriberHandler& other) = delete;
        EventSubscriberHandler& operator=(const EventSubscriberHandler& other) = delete;
        EventSubscriberHandler(EventSubscriberHandler&& other) = delete;
        EventSubscriberHandler& operator=(EventSubscriberHandler&& other) = delete;
        ~EventSubscriberHandler();

        void unregister() const override;

        [[nodiscard]] const EventHandlerIdentifier id() const noexcept override;

    private:
        EventHandlerIdentifier identifier_;
        EventAggregator* event_hub_;
	};
}