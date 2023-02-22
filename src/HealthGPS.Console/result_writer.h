#pragma once

#include "HealthGPS/result_message.h"

namespace host
{
	/// @brief Defines the Health-GPS results message writer interface
	class ResultWriter {
	public:
		/// @brief Destroys a host::ResultWriter instance
		virtual ~ResultWriter() = default;

		/// @brief Writes the hgps::ResultEventMessage contents to a stream
		/// @param message The message instance to process
		virtual void write(const hgps::ResultEventMessage& message) = 0;
	};
}
