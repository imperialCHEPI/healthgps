#pragma once
#include "event_message.h"

namespace hgps {

	/// @brief Implements the simulation error event message data type
	struct ErrorEventMessage final : public EventMessage {

		ErrorEventMessage() = delete;

		/// @brief Initialises a new instance of the ErrorEventMessage structure.
		/// @param sender The sender identifier
		/// @param run Current simulation run number
		/// @param time Current simulation time
		/// @param what The associated error message
		ErrorEventMessage(std::string sender, unsigned int run, int time, std::string what) noexcept;
		
		/// @brief Gets the associate Simulation time
		const int model_time{};

		/// @brief Gets the error message
		const std::string message;

		int id() const noexcept override;

		std::string to_string() const  override;

		void accept(EventMessageVisitor& visitor) const override;
	};
}

