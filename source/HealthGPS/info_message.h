#pragma once
#include "event_message.h"

namespace hgps {

	enum class ModelAction { start, update, stop };

	struct InfoEventMessage final : public EventMessage {

		InfoEventMessage() = delete;
		InfoEventMessage(std::string sender, ModelAction action, unsigned int run, int time) noexcept;
		InfoEventMessage(std::string sender, ModelAction action, unsigned int run, int time, std::string msg) noexcept;
		
		const int model_time{};

		const ModelAction model_action{};

		const std::string message;

		int id() const noexcept override;

		std::string to_string() const  override;

		void accept(EventMessageVisitor& visitor) const override;
	};

	namespace detail {
		/// @brief Converts enumeration to string, not pretty but no support in C++
		const static std::string model_action_str(const ModelAction action) {
			switch (action)
			{
				case ModelAction::update: return "update";
				case ModelAction::start:  return "start";
				case ModelAction::stop:   return "stop";
				default: 				  return "unknown";
			}
		}
	}
}


