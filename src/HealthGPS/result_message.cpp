#include "result_message.h"
#include <fmt/format.h>

namespace hgps {

	ResultEventMessage::ResultEventMessage(
		std::string sender, unsigned int run, int time, ModelResult result)
		: EventMessage{sender, run}, model_time{time}, content{result} {}

	int ResultEventMessage::id() const noexcept {
		return static_cast<int>(EventType::result);
	}

	std::string ResultEventMessage::to_string() const {
		return fmt::format("Source: {}, run # {}, time: {}, results:\n{}",
			source, run_number, model_time, content.to_string());
	}
	void ResultEventMessage::accept(EventMessageVisitor& visitor) const	{
		visitor.visit(*this);
	}
}