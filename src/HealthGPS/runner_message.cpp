#include "runner_message.h"
#include <fmt/format.h>

#include <utility>

hgps::RunnerEventMessage::RunnerEventMessage(std::string sender, RunnerAction run_action) noexcept
    : RunnerEventMessage(std::move(sender), run_action, 0u, 0.0) {}

hgps::RunnerEventMessage::RunnerEventMessage(std::string sender, RunnerAction run_action,
                                             double elapsed) noexcept
    : RunnerEventMessage(std::move(sender), run_action, 0u, elapsed) {}

hgps::RunnerEventMessage::RunnerEventMessage(std::string sender, RunnerAction run_action,
                                             unsigned int run) noexcept
    : RunnerEventMessage(std::move(sender), run_action, run, 0.0) {}

hgps::RunnerEventMessage::RunnerEventMessage(std::string sender, RunnerAction run_action,
                                             unsigned int run, double elapsed) noexcept
    : EventMessage{std::move(sender), run}, action{run_action}, elapsed_ms{elapsed} {}

int hgps::RunnerEventMessage::id() const noexcept { return static_cast<int>(EventType::runner); }

std::string hgps::RunnerEventMessage::to_string() const {
    if (action == RunnerAction::start) {
        return fmt::format("Source: {}, experiment started ...", source);
    }

    if (action == RunnerAction::run_begin) {
        return fmt::format("Source: {}, run # {} began ...", source, run_number);
    }

    if (action == RunnerAction::run_end) {
        return fmt::format("Source: {}, run # {} ended in {}ms.", source, run_number, elapsed_ms);
    }

    return fmt::format("Source: {}, experiment finished in {}ms.", source, elapsed_ms);
}

void hgps::RunnerEventMessage::accept(EventMessageVisitor &visitor) const { visitor.visit(*this); }
