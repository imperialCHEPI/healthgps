#pragma once
#include "event_message.h"

namespace hgps {

	struct ErrorEventMessage final : public EventMessage {

		ErrorEventMessage() = delete;
		ErrorEventMessage(std::string sender, unsigned int run, int time, std::string what) noexcept;

		const int model_time{};

		const std::string message;

		int id() const noexcept override;

		std::string to_string() const  override;

		void accept(EventMessageVisitor& visitor) const override;
	};
}

