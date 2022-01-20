#pragma once

#include <string>
#include <fmt/format.h>

struct ModelInfo
{
	std::string name;
	std::string version;
	std::string to_string() const noexcept {
		return fmt::format("{} v{}", name, version);
	}
};
