#pragma once

#include "HealthGPS\result_message.h"

class ResultWriter {
public:
	virtual ~ResultWriter() = default;
	virtual void write(const hgps::ResultEventMessage& message) = 0;
};
