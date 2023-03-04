#pragma once
#include "event_message.h"

namespace hgps {

	/// @brief Enumerates simulation model actions
	enum class ModelAction 
	{
		/// @brief Simulation has started, time = start
		start,

		/// @brief Simulation time has updated, time = time + 1
		update,

		/// @brief Simulation has stopped, time = end
		stop
	};

	struct InfoEventMessage final : public EventMessage {

		InfoEventMessage() = delete;
		InfoEventMessage(std::string sender, ModelAction action, unsigned int run, int time) noexcept;
		InfoEventMessage(std::string sender, ModelAction action, unsigned int run, int time, std::string msg) noexcept;

		const ModelAction model_action{};

		const int model_time{};

		const std::string message;

		int id() const noexcept override;

		std::string to_string() const  override;

		void accept(EventMessageVisitor& visitor) const override;
	};

	namespace detail {
		/// @brief Converts enumeration to string, not pretty but no support in C++
		const std::string model_action_str(const ModelAction action);
	}
}
