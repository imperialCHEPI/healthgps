#include "event_monitor.h"

#include "HealthGPS/error_message.h"
#include "HealthGPS/info_message.h"
#include "HealthGPS/runner_message.h"

#include <fmt/color.h>
#include <fmt/core.h>

namespace host {
EventMonitor::EventMonitor(hgps::EventAggregator &event_bus, ResultWriter &result_writer)
    : result_writer_{result_writer}, threads_{}, handlers_{}, info_queue_{}, results_queue_{},
      cancel_source_{} {
    handlers_.emplace_back(
        event_bus.subscribe(hgps::EventType::runner, std::bind(&EventMonitor::info_event_handler,
                                                               this, std::placeholders::_1)));

    handlers_.emplace_back(
        event_bus.subscribe(hgps::EventType::info, std::bind(&EventMonitor::info_event_handler,
                                                             this, std::placeholders::_1)));

    handlers_.emplace_back(
        event_bus.subscribe(hgps::EventType::result, std::bind(&EventMonitor::result_event_handler,
                                                               this, std::placeholders::_1)));

    handlers_.emplace_back(
        event_bus.subscribe(hgps::EventType::error, std::bind(&EventMonitor::error_event_handler,
                                                              this, std::placeholders::_1)));

    threads_.emplace_back(
        std::jthread(&EventMonitor::info_dispatch_thread, this, cancel_source_.get_token()));
    threads_.emplace_back(
        std::jthread(&EventMonitor::result_dispatch_thread, this, cancel_source_.get_token()));
}

EventMonitor::~EventMonitor() noexcept {
    for (auto &handler : handlers_) {
        handler->unsubscribe();
    }

    stop();
}

void EventMonitor::stop() noexcept {
    cancel_source_.request_stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void EventMonitor::visit(const hgps::RunnerEventMessage &message) {
    fmt::print(fg(fmt::color::cornflower_blue), "{}\n", message.to_string());
}

void EventMonitor::visit(const hgps::InfoEventMessage &message) {
    fmt::print(fg(fmt::color::light_blue), "{}\n", message.to_string());
}

void EventMonitor::visit(const hgps::ErrorEventMessage &message) {
    fmt::print(fg(fmt::color::red), "{}\n", message.to_string());
}

void EventMonitor::visit(const hgps::ResultEventMessage &message) { result_writer_.write(message); }

void EventMonitor::info_event_handler(std::shared_ptr<hgps::EventMessage> message) {
    info_queue_.push(message);
}

void EventMonitor::error_event_handler(std::shared_ptr<hgps::EventMessage> message) {
    // handle error synchronous, no delay!
    message->accept(*this);
}

void EventMonitor::result_event_handler(std::shared_ptr<hgps::EventMessage> message) {
    results_queue_.push(message);
}

void EventMonitor::info_dispatch_thread(std::stop_token token) {
    fmt::print(fg(fmt::color::light_blue), "Info event thread started...\n");
    while (!token.stop_requested()) {
        auto m = info_queue_.pop();
        if (m.has_value()) {
            m.value()->accept(*this);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    fmt::print(fg(fmt::color::light_blue), "Info event thread exited.\n");
}

void EventMonitor::result_dispatch_thread(std::stop_token token) {
    fmt::print(fg(fmt::color::gray), "Result event thread started...\n");
    while (!token.stop_requested()) {
        auto m = results_queue_.pop();
        if (m.has_value()) {
            m.value()->accept(*this);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    fmt::print(fg(fmt::color::gray), "Result event thread exited.\n");
}
} // namespace host