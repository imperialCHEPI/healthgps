#pragma once
#include "event_aggregator.h"

namespace hgps {

	/// @brief Implements the event subscriber handler data type
	class EventSubscriberHandler final: public EventSubscriber {
    public:
        EventSubscriberHandler() = delete;

        /// @brief Initialise a new instance of the EventSubscriberHandler class
        /// @param id The event handler identifier
        /// @param hub The event aggregator instance to subscribe
        /// @throws std::invalid_argument for null event aggregator hub argument
        EventSubscriberHandler(EventHandlerIdentifier id, EventAggregator* hub);
        
        EventSubscriberHandler(const EventSubscriberHandler& other) = delete;
        EventSubscriberHandler& operator=(const EventSubscriberHandler& other) = delete;
        EventSubscriberHandler(EventSubscriberHandler&& other) = delete;
        EventSubscriberHandler& operator=(EventSubscriberHandler&& other) = delete;

        /// @brief Destroys a EventSubscriberHandler instance
        ~EventSubscriberHandler();

        void unsubscribe() const override;

        [[nodiscard]] const EventHandlerIdentifier id() const noexcept override;

    private:
        EventHandlerIdentifier identifier_;
        EventAggregator* event_hub_;
	};
}