#include "event_monitor.h"

#include "HealthGPS/error_message.h"
#include "HealthGPS/info_message.h"
#include "HealthGPS/runner_message.h"

#include <fmt/color.h>
#include <fmt/core.h>

namespace host {
EventMonitor::EventMonitor(hgps::EventAggregator &event_bus, ResultWriter &result_writer)
    : result_writer_{result_writer}, tg_{tg_context_} {
    handlers_.emplace_back(event_bus.subscribe(hgps::EventType::runner, [this](auto &&PH1) {
        info_event_handler(std::forward<decltype(PH1)>(PH1));
    }));

    handlers_.emplace_back(event_bus.subscribe(hgps::EventType::info, [this](auto &&PH1) {
        info_event_handler(std::forward<decltype(PH1)>(PH1));
    }));

    handlers_.emplace_back(event_bus.subscribe(hgps::EventType::result, [this](auto &&PH1) {
        result_event_handler(std::forward<decltype(PH1)>(PH1));
    }));

    handlers_.emplace_back(event_bus.subscribe(hgps::EventType::error, [this](auto &&PH1) {
        error_event_handler(std::forward<decltype(PH1)>(PH1));
    }));

    tg_.run([this] { info_dispatch_thread(); });
    tg_.run([this] { result_dispatch_thread(); });
}

EventMonitor::~EventMonitor() noexcept {
    for (auto &handler : handlers_) {
        handler->unsubscribe();
    }

    stop();
}

void EventMonitor::stop() noexcept {
    tg_context_.cancel_group_execution();
    tg_.wait();
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
    info_queue_.emplace(std::move(message));
}

void EventMonitor::error_event_handler(const std::shared_ptr<hgps::EventMessage> &message) {
    // handle error synchronous, no delay!
    message->accept(*this);
}

void EventMonitor::result_event_handler(std::shared_ptr<hgps::EventMessage> message) {
    results_queue_.emplace(std::move(message));
}

void EventMonitor::info_dispatch_thread() {
    fmt::print(fg(fmt::color::light_blue), "Info event thread started...\n");
    while (!tg_context_.is_group_execution_cancelled()) {
        std::shared_ptr<hgps::EventMessage> m;
        if (info_queue_.try_pop(m)) {
            m->accept(*this);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    fmt::print(fg(fmt::color::light_blue), "Info event thread exited.\n");
}

void EventMonitor::result_dispatch_thread() {
    fmt::print(fg(fmt::color::gray), "Result event thread started...\n");
    while (!tg_context_.is_group_execution_cancelled()) {
        std::shared_ptr<hgps::EventMessage> m;
        if (results_queue_.try_pop(m)) {
            m->accept(*this);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    fmt::print(fg(fmt::color::gray), "Result event thread exited.\n");
}
} // namespace host
