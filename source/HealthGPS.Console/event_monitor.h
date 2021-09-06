#pragma once
#include <thread>

#include "HealthGPS/event_aggregator.h"
#include "HealthGPS/threadsafe_queue.h"

#include "result_writer.h"

class EventMonitor final : public hgps::EventMessageVisitor {
public:
	EventMonitor() = delete;
	EventMonitor(hgps::EventAggregator& event_bus, ResultWriter& result_writer);
	~EventMonitor() noexcept;

	void stop() noexcept;

	void visit(const hgps::RunnerEventMessage& message) override;
	void visit(const hgps::InfoEventMessage& message) override;
	void visit(const hgps::ErrorEventMessage& message) override;
	void visit(const hgps::ResultEventMessage& message) override;

private:
	ResultWriter& result_writer_;
	std::vector<std::jthread> threads_;
	std::vector <std::unique_ptr<hgps::EventSubscriber>> handlers_;
	hgps::ThreadsafeQueue < std::shared_ptr<hgps::EventMessage>> info_queue_;
	hgps::ThreadsafeQueue < std::shared_ptr<hgps::EventMessage>> results_queue_;

	void info_event_handler(std::shared_ptr<hgps::EventMessage> message);
	void error_event_handler(std::shared_ptr<hgps::EventMessage> message);
	void result_event_handler(std::shared_ptr<hgps::EventMessage> message);
};

