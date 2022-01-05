#include "error_message.h"
#include <fmt/format.h>

namespace hgps {

	ErrorEventMessage::ErrorEventMessage(std::string sender,
		unsigned int run, int time, std::string what) noexcept
		: EventMessage{ sender, run },  model_time{ time }, message{ what } {}

	int ErrorEventMessage::id() const noexcept {
		return static_cast<int>(EventType::error);
	}

	std::string ErrorEventMessage::to_string() const {
		auto formatting = std::string{ "Source: {}, run # {}, {}, time: {}, cause: {}" };
		return fmt::format(formatting, source, run_number, model_time, message);
	}

	void ErrorEventMessage::accept(EventMessageVisitor& visitor) const {
		visitor.visit(*this);
	}
}