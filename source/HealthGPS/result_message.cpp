#include "result_message.h"
#include <format>

namespace hgps {

	ResultEventMessage::ResultEventMessage(
		std::string sender, unsigned int run, int time, ModelResult result)
		: EventMessage{sender, run}, model_time{time}, content{result} {}

	int ResultEventMessage::id() const noexcept {
		return static_cast<int>(EventType::result);
	}

	std::string ResultEventMessage::to_string() const {
		return std::format("Source: {}, run # {}, time: {}\n{}",
			source, run_number, model_time, content.to_string());
	}
}