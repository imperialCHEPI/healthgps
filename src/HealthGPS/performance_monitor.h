#pragma once

#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <fmt/format.h>
#include <fmt/color.h>

namespace hgps {

/// @brief Simple performance monitoring utility
class PerformanceMonitor {
public:
    /// @brief Start timing a named operation
    /// @param operation_name Name of the operation being timed
    void start_operation(const std::string& operation_name);

    /// @brief End timing the current operation
    void end_operation();

    /// @brief Record a completed operation with its duration
    /// @param operation_name Name of the operation
    /// @param duration_ms Duration in milliseconds
    void record_operation(const std::string& operation_name, double duration_ms);

    /// @brief Print performance summary
    void print_summary() const;

    /// @brief Get average time for an operation
    /// @param operation_name Name of the operation
    /// @return Average time in milliseconds, or 0 if not found
    double get_average_time(const std::string& operation_name) const;

    /// @brief Clear all recorded metrics
    void clear();

private:
    struct OperationMetrics {
        std::vector<double> times;
        double total_time = 0.0;
        size_t count = 0;
        
        void add_time(double time_ms) {
            times.push_back(time_ms);
            total_time += time_ms;
            count++;
        }
        
        double average() const {
            return count > 0 ? total_time / count : 0.0;
        }
    };

    mutable std::mutex metrics_mutex_;
    std::unordered_map<std::string, OperationMetrics> operations_;
    
    // Current operation tracking
    std::string current_operation_;
    std::chrono::steady_clock::time_point current_start_;
};

/// @brief RAII timer for automatic operation timing
class ScopedTimer {
public:
    /// @brief Start timing an operation
    /// @param monitor Performance monitor to use
    /// @param operation_name Name of the operation
    ScopedTimer(PerformanceMonitor& monitor, const std::string& operation_name);

    /// @brief End timing automatically when destroyed
    ~ScopedTimer();

    // Non-copyable, movable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = default;
    ScopedTimer& operator=(ScopedTimer&&) = default;

private:
    PerformanceMonitor& monitor_;
    std::chrono::steady_clock::time_point start_time_;
    std::string operation_name_;
};

/// @brief Macro for easy scoped timing
#define PERF_TIMER(monitor, name) \
    hgps::ScopedTimer _timer(monitor, name)

/// @brief Global performance monitor instance
extern PerformanceMonitor global_performance_monitor;

} // namespace hgps 