#include "event_monitor.h"

#include "HealthGPS\info_message.h"
#include "HealthGPS\runner_message.h"
#include "HealthGPS\error_message.h"

#include <fmt/core.h>
#include <fmt/color.h>

EventMonitor::EventMonitor(hgps::EventAggregator& event_bus, ResultWriter& result_writer)
	: result_writer_{ result_writer }, threads_{}, handlers_{}, info_queue_{}, results_queue_{}
{
	handlers_.emplace_back(event_bus.subscribe(hgps::EventType::runner,
		std::bind(&EventMonitor::info_event_handler, this, std::placeholders::_1)));

	handlers_.emplace_back(event_bus.subscribe(hgps::EventType::info, 
		std::bind(&EventMonitor::info_event_handler, this, std::placeholders::_1)));

	handlers_.emplace_back(event_bus.subscribe(hgps::EventType::result, 
		std::bind(&EventMonitor::result_event_handler, this, std::placeholders::_1)));

	handlers_.emplace_back(event_bus.subscribe(hgps::EventType::error,
		std::bind(&EventMonitor::error_event_handler, this, std::placeholders::_1)));

	threads_.emplace_back(std::jthread([&](std::stop_token stoken) {
		fmt::print(fg(fmt::color::light_blue), "Info event thread started...\n");
		while (!stoken.stop_requested()) {
			auto m = info_queue_.pop();
			if (m.has_value()) {
				m.value()->accept(*this);
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		fmt::print(fg(fmt::color::light_blue), "Info event thread exited.\n");
	}));

	threads_.emplace_back(std::jthread([&](std::stop_token stoken) {
		fmt::print(fg(fmt::color::gray), "Result event thread started...\n");
		while (!stoken.stop_requested()) {
			auto m = results_queue_.pop();
			if (m.has_value()) {
				m.value()->accept(*this);
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		fmt::print(fg(fmt::color::gray), "Result event thread exited.\n");
	}));
}

EventMonitor::~EventMonitor() noexcept {
	for (auto& handler : handlers_) {
		handler->unregister();
	}

	stop();
}

void EventMonitor::stop() noexcept {
	for (auto& thread : threads_) {
		thread.request_stop();
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void EventMonitor::visit(const hgps::RunnerEventMessage& message) {
	fmt::print(fg(fmt::color::cornflower_blue), "{}\n", message.to_string() );
}

void EventMonitor::visit(const hgps::InfoEventMessage& message) {
	fmt::print(fg(fmt::color::light_blue), "{}\n", message.to_string());
}

void EventMonitor::visit(const hgps::ErrorEventMessage& message) {
	fmt::print(fg(fmt::color::red), "{}\n", message.to_string());
}

void EventMonitor::visit(const hgps::ResultEventMessage& message) {
	result_writer_.write(message);
}

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
