#include "info_message.h"
#include <fmt/format.h>

#include <utility>

namespace hgps {

InfoEventMessage::InfoEventMessage(std::string sender, ModelAction action, unsigned int run,
                                   int time) noexcept
    : InfoEventMessage(sender, action, run, time, std::string{}) {}

InfoEventMessage::InfoEventMessage(std::string sender, ModelAction action, unsigned int run,
                                   int time, std::string msg) noexcept
    : EventMessage{sender, run}, model_action{action}, model_time{time}, message{std::move(msg)} {}

int InfoEventMessage::id() const noexcept { return static_cast<int>(EventType::info); }

std::string InfoEventMessage::to_string() const {
    std::string result{};
    if (message.empty()) {
        result = fmt::format("Source: {}, run # {}, {}, time: {}", source, run_number,
                             detail::model_action_str(model_action), model_time);
    }

    result = fmt::format("Source: {}, run # {}, {}, time: {} - {}", source, run_number,
                         detail::model_action_str(model_action), model_time, message);

    return result;
}

void InfoEventMessage::accept(EventMessageVisitor &visitor) const { visitor.visit(*this); }

namespace detail {

std::string model_action_str(ModelAction action) {
    switch (action) {
    case ModelAction::update:
        return "update";
    case ModelAction::start:
        return "start";
    case ModelAction::stop:
        return "stop";
    default:
        return "unknown";
    }
}
} // namespace detail
} // namespace hgps
