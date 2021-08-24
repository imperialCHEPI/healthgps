#pragma once
#include "event_message.h"

namespace hgps {

	enum class RunnerAction	{start, run_begin, run_end, finish };

	struct RunnerEventMessage final : public EventMessage {

		RunnerEventMessage() = delete;
		RunnerEventMessage(std::string sender, RunnerAction run_action) noexcept;
		RunnerEventMessage(std::string sender, RunnerAction run_action, double elapsed) noexcept;
		RunnerEventMessage(std::string sender, RunnerAction run_action, unsigned int run) noexcept;
		RunnerEventMessage(std::string sender, RunnerAction run_action, unsigned int run, double elapsed) noexcept;

		const RunnerAction action{};

		const double elapsed_ms{};

		int id() const noexcept override;

		std::string to_string() const  override;
	};
}

