#include "failure_report.h"

#include "HealthGPS.Core/exception.h"

#include <cstdio>
#include <fmt/color.h>
#include <fmt/format.h>

namespace hgps {
namespace {

// MAHIMA: One phase path per thread. Worker threads must not share this with main.
thread_local std::string g_simulation_phase;

[[nodiscard]] const char *exception_type_name(const std::exception &ex) {
    if (dynamic_cast<const core::HgpsException *>(&ex) != nullptr) {
        return "hgps::core::HgpsException";
    }
    if (dynamic_cast<const std::out_of_range *>(&ex) != nullptr) {
        return "std::out_of_range";
    }
    if (dynamic_cast<const std::invalid_argument *>(&ex) != nullptr) {
        return "std::invalid_argument";
    }
    if (dynamic_cast<const std::logic_error *>(&ex) != nullptr) {
        return "std::logic_error";
    }
    if (dynamic_cast<const std::runtime_error *>(&ex) != nullptr) {
        return "std::runtime_error";
    }
    return "std::exception";
}

[[nodiscard]] std::string phase_or_unknown(std::string_view phase) {
    return phase.empty() ? std::string{"unknown phase"} : std::string{phase};
}

} // namespace

SimulationPhase::SimulationPhase(std::string name)
    : uncaught_on_enter_{std::uncaught_exceptions()} {
    // MAHIMA: Keep the parent path so the banner reads outer > inner, not only the leaf.
    previous_ = g_simulation_phase;
    if (previous_.empty()) {
        g_simulation_phase = std::move(name);
        return;
    }
    g_simulation_phase = previous_ + " > " + name;
}

SimulationPhase::~SimulationPhase() {
    // MAHIMA: Unwind keeps the failing path. A normal return restores the parent path.
    if (std::uncaught_exceptions() > uncaught_on_enter_) {
        return;
    }
    g_simulation_phase = std::move(previous_);
}

std::string current_simulation_phase() { return g_simulation_phase; }

void clear_simulation_phase() { g_simulation_phase.clear(); }

void validate_requested_thread_count(std::size_t requested) {
    if (requested == 0 || requested >= minimum_explicit_thread_count) {
        return;
    }
    throw std::invalid_argument(fmt::format(
        "Option -T {} is below the minimum of {} threads. Health-GPS keeps two long-running "
        "tasks (info events and result tracking) as well as the simulation. Fewer than {} "
        "threads stops the run with no result. Use -T {} or higher, or omit -T to use every "
        "core (0 means no cap).",
        requested, minimum_explicit_thread_count, minimum_explicit_thread_count,
        minimum_explicit_thread_count));
}

std::string format_failure(std::string_view phase, const std::exception &ex) {
    return fmt::format("Health-GPS stopped during [{}].\nReason ({}): {}", phase_or_unknown(phase),
                       exception_type_name(ex), ex.what());
}

std::string format_unknown_failure(std::string_view phase) {
    return fmt::format(
        "Health-GPS stopped during [{}].\nReason: an exception that does not derive from "
        "std::exception was thrown, so it has no message.",
        phase_or_unknown(phase));
}

void report_failure(std::string_view message) {
    // MAHIMA: Flush first so lines printed just before the throw are not stuck in the buffer.
    std::fflush(stdout);
    fmt::print(fg(fmt::color::red), "\n{}\n", message);
    std::fflush(stdout);
}

void install_failure_terminate_handler() {
    std::set_terminate([] {
        const auto phase = current_simulation_phase();
        std::string message = "Health-GPS called std::terminate with no C++ exception attached.";
        if (const auto eptr = std::current_exception()) {
            try {
                std::rethrow_exception(eptr);
            } catch (const std::exception &ex) {
                message = format_failure(phase, ex);
            } catch (...) {
                message = format_unknown_failure(phase);
            }
        } else if (!phase.empty()) {
            message = fmt::format(
                "Health-GPS called std::terminate during [{}]. No C++ exception is attached. "
                "This usually means a noexcept function threw, or a worker thread aborted.",
                phase);
        }
        std::fflush(stdout);
        std::fputs("\n", stderr);
        std::fputs(message.c_str(), stderr);
        std::fputs("\n", stderr);
        std::fflush(stderr);
        std::abort();
    });
}

} // namespace hgps
