#include "event_subscriber.h"

#include <utility>

namespace hgps {
EventSubscriberHandler::EventSubscriberHandler(EventHandlerIdentifier id, EventAggregator *hub)
    : identifier_{std::move(id)}, event_hub_{hub} {

    if (hub == nullptr) {
        throw std::invalid_argument("The event aggregator hub argument must not be null.");
    }
}

EventSubscriberHandler::~EventSubscriberHandler() { unsubscribe(); }

void EventSubscriberHandler::unsubscribe() const {
    if (event_hub_) {
        event_hub_->unsubscribe(*this);
    }
}
EventHandlerIdentifier EventSubscriberHandler::id() const noexcept { return identifier_; }
} // namespace hgps
