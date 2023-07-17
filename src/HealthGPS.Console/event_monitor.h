#pragma once
#include <thread>

#include <oneapi/tbb/concurrent_queue.h>

#include "HealthGPS/event_aggregator.h"

#include "result_writer.h"

namespace host {
/// @brief Defined the event monitor class used for processing Health-GPS event messages
///
/// All notification messages are written to the terminal, while the results messages are
/// queued to be processed by the host::ResultWriter instance provide at construction.
class EventMonitor final : public hgps::EventMessageVisitor {
  public:
    EventMonitor() = delete;

    /// @brief Initialises a new instance of the host::EventMonitor class.
    /// @param event_bus The message bus instance to monitor
    /// @param result_writer The results message writer instance
    EventMonitor(hgps::EventAggregator &event_bus, ResultWriter &result_writer);

    /// @brief Destroys a host::EventMonitor instance
    ~EventMonitor() noexcept;

    /// @brief Stops the monitor, no new messages are processed after stop
    void stop() noexcept;

    void visit(const hgps::RunnerEventMessage &message) override;
    void visit(const hgps::InfoEventMessage &message) override;
    void visit(const hgps::ErrorEventMessage &message) override;
    void visit(const hgps::ResultEventMessage &message) override;

  private:
    ResultWriter &result_writer_;
    std::vector<std::jthread> threads_;
    std::vector<std::unique_ptr<hgps::EventSubscriber>> handlers_;
    tbb::concurrent_queue<std::shared_ptr<hgps::EventMessage>> info_queue_;
    tbb::concurrent_queue<std::shared_ptr<hgps::EventMessage>> results_queue_;
    std::stop_source cancel_source_;

    void info_event_handler(std::shared_ptr<hgps::EventMessage> message);
    void error_event_handler(std::shared_ptr<hgps::EventMessage> message);
    void result_event_handler(std::shared_ptr<hgps::EventMessage> message);

    void info_dispatch_thread(std::stop_token token);
    void result_dispatch_thread(std::stop_token token);
};
} // namespace host