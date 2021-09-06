#include "result_file_writer.h"

#include <fmt/core.h>
#include <fmt/color.h>

void ResultFileWriter::write(const hgps::ResultEventMessage& message) {
	fmt::print(fg(fmt::color::gray), "{}\n", message.to_string());
}
