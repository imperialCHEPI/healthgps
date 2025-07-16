#include "performance_monitor.h"
#include <algorithm>
#include <iomanip>
#include <iostream>

namespace hgps {

// Global performance monitor instance
PerformanceMonitor global_performance_monitor;

void PerformanceMonitor::start_operation(const std::string &operation_name) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    current_operation_ = operation_name;
    current_start_ = std::chrono::steady_clock::now();
}

void PerformanceMonitor::end_operation() {
    auto end_time = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    if (!current_operation_.empty()) {
        auto duration = std::chrono::duration<double, std::milli>(end_time - current_start_);
        operations_[current_operation_].add_time(duration.count());
        current_operation_.clear();
    }
}

void PerformanceMonitor::record_operation(const std::string &operation_name, double duration_ms) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    operations_[operation_name].add_time(duration_ms);
}

void PerformanceMonitor::print_summary() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    if (operations_.empty()) {
        fmt::print(fmt::fg(fmt::color::yellow), "\n No performance data recorded\n");
        return;
    }

    fmt::print(fmt::fg(fmt::color::cyan), "\n Performance Summary:\n");
    fmt::print(fmt::fg(fmt::color::cyan), "{':<30} {':<10} {':<12} {':<12} {':<12}\n", "Operation",
               "Count", "Total(ms)", "Avg(ms)", "Max(ms)");
    fmt::print(fmt::fg(fmt::color::cyan), "{}\n", std::string(80, '-'));

    // Sort operations by total time (descending)
    std::vector<std::pair<std::string, OperationMetrics>> sorted_ops(operations_.begin(),
                                                                     operations_.end());
    std::sort(sorted_ops.begin(), sorted_ops.end(), [](const auto &a, const auto &b) {
        return a.second.total_time > b.second.total_time;
    });

    for (const auto &[name, metrics] : sorted_ops) {
        double max_time = metrics.times.empty()
                              ? 0.0
                              : *std::max_element(metrics.times.begin(), metrics.times.end());

        fmt::print("{:<30} {:<10} {:<12.2f} {:<12.2f} {:<12.2f}\n", name, metrics.count,
                   metrics.total_time, metrics.average(), max_time);
    }

    fmt::print("\n");
}

double PerformanceMonitor::get_average_time(const std::string &operation_name) const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    auto it = operations_.find(operation_name);
    return it != operations_.end() ? it->second.average() : 0.0;
}

void PerformanceMonitor::clear() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    operations_.clear();
    current_operation_.clear();
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(PerformanceMonitor &monitor, const std::string &operation_name)
    : monitor_(monitor), operation_name_(operation_name) {
    start_time_ = std::chrono::steady_clock::now();
}

ScopedTimer::~ScopedTimer() {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end_time - start_time_);
    monitor_.record_operation(operation_name_, duration.count());
}

} // namespace hgps
