#pragma once
#include "event_message.h"
#include "model_result.h"

namespace hgps {

	struct ResultEventMessage final : public EventMessage {

		ResultEventMessage() = delete;
		ResultEventMessage(std::string sender, unsigned int run, int time, ModelResult result);

		const int model_time{};

		const ModelResult content{};

		int id() const noexcept override;

		std::string to_string() const  override;
	};
}
