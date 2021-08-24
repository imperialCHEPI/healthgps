#include "info_message.h"
#include <format>
#include <map>

namespace hgps {

	InfoEventMessage::InfoEventMessage(std::string sender, ModelAction action,
		unsigned int run, int time) noexcept
		: InfoEventMessage(sender, action, run, time, std::string{}) {}

	InfoEventMessage::InfoEventMessage(std::string sender, ModelAction action,
		unsigned int run, int time, std::string msg) noexcept
		: source{sender}, model_action{ action }, run_number{run}, model_time{time}, message{msg} {}

	int InfoEventMessage::id() const noexcept {
		return static_cast<int>(EventType::info);
	}

	std::string InfoEventMessage::to_string() const	{
		auto formatting = std::string{ "Source: {}, run # {}, action: {}, time: {}" };
		formatting += message.empty() ? "{}" : " - {}";

		return std::format(formatting, source, run_number,
			detail::model_action_str(model_action), model_time, message);
	}
}


