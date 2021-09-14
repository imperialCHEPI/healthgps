#pragma once

#include <string>
#include <format>

struct ModelInfo
{
	std::string name;
	std::string version;
	std::string to_string() const noexcept {
		return std::format("{} v{}", name, version);
	}
};
