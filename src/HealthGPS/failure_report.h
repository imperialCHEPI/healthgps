#pragma once

#include <exception>
#include <string>
#include <string_view>

namespace hgps {

/// @brief Smallest `-T` value Health-GPS accepts when a cap is requested.
/// @details `0` means "no cap" and is allowed. `1` is rejected.
inline constexpr std::size_t minimum_explicit_thread_count = 2;

/// @brief RAII label for the simulation step currently running on this thread.
///
/// @details If the scope exits because an exception is in flight, the label stays so the
/// runner and the terminate handler can print where the run stopped.
class SimulationPhase {
  public:
    explicit SimulationPhase(std::string name);
    ~SimulationPhase();

    SimulationPhase(const SimulationPhase &) = delete;
    SimulationPhase &operator=(const SimulationPhase &) = delete;
    SimulationPhase(SimulationPhase &&) = delete;
    SimulationPhase &operator=(SimulationPhase &&) = delete;

  private:
    std::string previous_;
    int uncaught_on_enter_;
};

/// @brief Phase path for this thread, or empty when no SimulationPhase is active.
[[nodiscard]] std::string current_simulation_phase();

/// @brief Drop the phase path. Used after a worker thread has stored the failure.
void clear_simulation_phase();

/// @brief Reject `-T` values that are set and below minimum_explicit_thread_count.
/// @param requested Thread cap from the command line. `0` means no cap.
/// @throws std::invalid_argument when requested is 1 (or any positive value below the minimum).
void validate_requested_thread_count(std::size_t requested);

/// @brief One paragraph naming the phase, the exception type, and ex.what().
[[nodiscard]] std::string format_failure(std::string_view phase, const std::exception &ex);

/// @brief Same banner for an exception that is not a std::exception.
[[nodiscard]] std::string format_unknown_failure(std::string_view phase);

/// @brief Write a failure banner and flush, so a crash cannot discard it.
void report_failure(std::string_view message);

/// @brief Last-resort printer for std::terminate (noexcept violations and escaped thread
/// exceptions).
void install_failure_terminate_handler();

} // namespace hgps
