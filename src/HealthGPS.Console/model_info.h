#pragma once

#include <fmt/format.h>
#include <string>

namespace hgps {
/// @brief Simulation experiment run-time information for reproducibility.
struct ExperimentInfo {
    /// @brief The model name
    std::string model;

    /// @brief The model version
    std::string version;

    /// @brief Intervention scenario identifier
    std::string intervention;

    /// @brief Simulation batch job identifier
    int job_id{};

    /// @brief Simulation initialisation seed value
    unsigned int seed{};

    /// @brief Creates a string representation of this instance
    /// @return The string representation
    std::string to_string() const noexcept {
        return fmt::format("{} v{} - {} job_id: {} seed: {}", model, version, intervention, job_id,
                           seed);
    }
};
} // namespace hgps
