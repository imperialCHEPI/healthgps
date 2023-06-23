#include "error_message.h"
#include <sstream>

namespace hgps {

ErrorEventMessage::ErrorEventMessage(std::string sender, unsigned int run, int time,
                                     std::string what) noexcept
    : EventMessage{sender, run}, model_time{time}, message{what} {}

int ErrorEventMessage::id() const noexcept { return static_cast<int>(EventType::error); }

std::string ErrorEventMessage::to_string() const {
    std::stringstream ss;
    ss << "Source: " << source << ", run # " << run_number;
    ss << ", time: " << model_time << ", cause : " << message;
    return ss.str();
}

void ErrorEventMessage::accept(EventMessageVisitor &visitor) const { visitor.visit(*this); }
} // namespace hgps