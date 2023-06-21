#pragma once
namespace hgps {

struct RunnerEventMessage;
struct InfoEventMessage;
struct ErrorEventMessage;
struct ResultEventMessage;

/// @brief Event message types visitor interface (double dispatcher)
class EventMessageVisitor {
  public:
    /// @brief Initialises a new instance of the visitor class
    EventMessageVisitor() = default;

    EventMessageVisitor(const EventMessageVisitor &) = delete;
    EventMessageVisitor &operator=(const EventMessageVisitor &) = delete;

    EventMessageVisitor(EventMessageVisitor &&) = delete;
    EventMessageVisitor &operator=(EventMessageVisitor &&) = delete;

    /// @brief Destroy an instance of the visitor class
    virtual ~EventMessageVisitor(){};

    /// @brief Visits a hgps::RunnerEventMessage message type
    /// @param message The message instance to visit
    virtual void visit(const RunnerEventMessage &message) = 0;

    /// @brief Visits a hgps::InfoEventMessage message  type
    /// @param message The message instance to visit
    virtual void visit(const InfoEventMessage &message) = 0;

    /// @brief Visits a hgps::ErrorEventMessage message  type
    /// @param message The message instance to visit
    virtual void visit(const ErrorEventMessage &message) = 0;

    /// @brief Visits a hgps::ResultEventMessage message  type
    /// @param message The message instance to visit
    virtual void visit(const ResultEventMessage &message) = 0;
};
} // namespace hgps