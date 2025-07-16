#pragma once

#include "event_aggregator.h"
#include "simulation.h"
#include "randombit_generator.h"

#include <atomic>
#include <memory>
#include <stop_token>
#include <vector>
#include <mutex>
#include <chrono>
#include <oneapi/tbb/task_group.h>
#include <oneapi/tbb/global_control.h>

namespace hgps {

/// @brief Configuration for optimized parallel trial execution
struct ParallelRunnerConfig {
    /// @brief Number of trials to run simultaneously
    unsigned int concurrent_trials = 4;
    
    /// @brief Maximum threads per individual trial
    unsigned int threads_per_trial = 16;
    
    /// @brief Enable detailed performance monitoring
    bool enable_monitoring = true;
    
    /// @brief Progress update interval in seconds
    double progress_interval = 10.0;
};

/// @brief Performance metrics for monitoring
struct PerformanceMetrics {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update;
    std::atomic<unsigned int> completed_trials{0};
    std::atomic<unsigned int> total_trials{0};
    std::atomic<double> avg_trial_time_ms{0.0};
    std::vector<double> trial_times;
    std::mutex metrics_mutex;
    
    void update_completion(double trial_time_ms);
    void print_progress();
    double estimated_time_remaining() const;
};

/// @brief Optimized simulation runner with parallel trial execution
class ParallelRunner {
public:
    ParallelRunner() = delete;

    /// @brief Initializes a new optimized ParallelRunner
    /// @param bus The message bus instance to use for notification
    /// @param seed_generator Master RNG for RNG seed generation
    /// @param config Configuration for parallel execution
    ParallelRunner(std::shared_ptr<EventAggregator> bus,
                   std::unique_ptr<RandomBitGenerator> seed_generator,
                   const ParallelRunnerConfig& config = {}) noexcept;

    /// @brief Run experiment with optimized parallel trial execution
    /// @param baseline The simulation engine instance
    /// @param trial_runs Total number of trials to execute
    /// @return The experiment total elapsed time, in milliseconds
    double run(Simulation& baseline, unsigned int trial_runs);

    /// @brief Run experiment for baseline and intervention scenarios in parallel
    /// @param baseline The simulation engine instance for baseline scenario
    /// @param intervention The simulation engine instance for intervention scenario  
    /// @param trial_runs Total number of trials to execute
    /// @return The experiment total elapsed time, in milliseconds
    double run(Simulation& baseline, Simulation& intervention, unsigned int trial_runs);

    /// @brief Gets a value indicating whether an experiment is currently running
    /// @return true if experiment is underway; otherwise, false
    bool is_running() const noexcept;

    /// @brief Cancel a running experiment
    void cancel() noexcept;

    /// @brief Get current performance metrics
    /// @return Current performance metrics
    const PerformanceMetrics& get_metrics() const noexcept;

private:
    std::atomic<bool> running_;
    std::shared_ptr<EventAggregator> event_bus_;
    std::unique_ptr<RandomBitGenerator> seed_generator_;
    std::stop_source source_;
    std::string runner_id_{};
    ParallelRunnerConfig config_;
    PerformanceMetrics metrics_;
    
    // Thread control for individual trials
    std::unique_ptr<tbb::global_control> trial_thread_control_;

    void run_trial_batch(const std::stop_token& token, 
                        Simulation& simulation,
                        const std::vector<std::pair<unsigned int, unsigned int>>& trial_seeds);
    
    void run_trial_worker(const std::stop_token& token,
                         Simulation& simulation, 
                         unsigned int run_number,
                         unsigned int seed);

    void monitor_progress();
    void notify(std::unique_ptr<EventMessage> message);
    
    // Optimize thread allocation for trials
    void setup_trial_threading();
    void cleanup_trial_threading();
};

} // namespace hgps 