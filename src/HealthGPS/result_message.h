#pragma once
#include "event_message.h"
#include "model_result.h"

namespace hgps {

	/// @brief Implements the simulation results event message data type
	struct ResultEventMessage final : public EventMessage {

		ResultEventMessage() = delete;

		/// @brief Initialises a new instance of the ResultEventMessage structure.
		/// @param sender The sender identifier
		/// @param run Current simulation run number
		/// @param time Current simulation time
		/// @param result The simulation results content
		ResultEventMessage(std::string sender, unsigned int run, int time, ModelResult result);

		/// @brief Gets the associated Simulation time
		int model_time{};

		/// @brief Gets the simulation results content
		ModelResult content;

		int id() const noexcept override;

		std::string to_string() const  override;

		void accept(EventMessageVisitor& visitor) const override;
	};
}
