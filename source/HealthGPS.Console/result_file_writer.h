#pragma once

#include "result_writer.h"

class ResultFileWriter final : public ResultWriter {
public:
	void write(const hgps::ResultEventMessage& message);
};

