#pragma once

#include <string>
#include <fmt/format.h>

struct ExperimentInfo
{
	std::string model;
	std::string version;
	std::string intervention;
	std::string to_string() const noexcept {
		return fmt::format("{} v{} - {}", model, version, intervention);
	}
};
