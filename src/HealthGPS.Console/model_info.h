#pragma once

#include <string>
#include <fmt/format.h>

struct ExperimentInfo
{
	std::string model;
	std::string version;
	std::string intervention;
	int job_id{};
	unsigned int seed{};
	std::string to_string() const noexcept {
		return fmt::format("{} v{} - {} job_id: {} seed: {}", model, version, intervention, job_id, seed);
	}
};
