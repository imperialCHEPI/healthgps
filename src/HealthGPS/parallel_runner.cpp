#include "parallel_runner.h"
#include "finally.h"
#include "runner_message.h"

#include <algorithm>
#include <chrono>
#include <fmt/color.h>
#include <fmt/format.h>
#include <iostream>
#include <oneapi/tbb/task_group.h>
#include <thread>

namespace hgps {

using ElapsedTime = std::chrono::duration<double, std::milli>;

// PerformanceMetrics implementation
void PerformanceMetrics::update_completion(double trial_time_ms) {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    trial_times.push_back(trial_time_ms);
    completed_trials++;

    // Update rolling average
    double sum = 0.0;
    for (double time : trial_times) {
        sum += time;
    }
    avg_trial_time_ms = sum / trial_times.size();
}

void PerformanceMetrics::print_progress() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - start_time).count();

    unsigned int completed = completed_trials.load();
    unsigned int total = total_trials.load();
    double avg_time = avg_trial_time_ms.load();

    double progress_percent = (total > 0) ? (100.0 * completed / total) : 0.0;
    double eta_seconds = estimated_time_remaining();

    fmt::print(fmt::fg(fmt::color::cyan),
               "\n🚀 Progress: {}/{} trials ({:.1f}%) | Avg: {:.1f}ms/trial | ETA: {:.1f}s | "
               "Elapsed: {:.1f}s",
               completed, total, progress_percent, avg_time, eta_seconds, elapsed);
}

double PerformanceMetrics::estimated_time_remaining() const {
    unsigned int completed = completed_trials.load();
    unsigned int total = total_trials.load();
    double avg_time = avg_trial_time_ms.load();

    if (completed == 0 || avg_time == 0.0)
        return 0.0;

    unsigned int remaining = total - completed;
    return (remaining * avg_time) / 1000.0; // Convert to seconds
}

// ParallelRunner implementation
ParallelRunner::ParallelRunner(std::shared_ptr<EventAggregator> bus,
                               std::unique_ptr<RandomBitGenerator> seed_generator,
                               const ParallelRunnerConfig &config) noexcept
    : running_{false}, event_bus_{std::move(bus)}, seed_generator_{std::move(seed_generator)},
      config_{config} {}

double ParallelRunner::run(Simulation &baseline, unsigned int trial_runs) {
    if (trial_runs < 1) {
        throw std::invalid_argument("The number of trial runs must not be less than one");
    }
    if (baseline.type() != ScenarioType::baseline) {
        throw std::invalid_argument(
            fmt::format("Simulation: '{}' cannot be evaluated alone", baseline.name()));
    }
    if (running_.load()) {
        throw std::invalid_argument("The parallel runner is already evaluating an experiment");
    }

    source_ = std::stop_source{};
    running_.store(true);
    auto reset = make_finally([this]() {
        running_.store(false);
        cleanup_trial_threading();
    });

    runner_id_ = "optimized_single_runner";
    setup_trial_threading();

    // Initialize performance metrics
    metrics_.start_time = std::chrono::steady_clock::now();
    metrics_.last_update = metrics_.start_time;
    metrics_.completed_trials = 0;
    metrics_.total_trials = trial_runs;
    metrics_.trial_times.clear();
    metrics_.trial_times.reserve(trial_runs);

    auto start = std::chrono::steady_clock::now();
    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::start));

    fmt::print(fmt::fg(fmt::color::yellow),
               "\n🔥 Starting OPTIMIZED execution: {} trials with {} concurrent workers\n",
               trial_runs, config_.concurrent_trials);

    // Generate all trial seeds upfront
    std::vector<std::pair<unsigned int, unsigned int>> trial_seeds;
    trial_seeds.reserve(trial_runs);
    for (auto run = 1u; run <= trial_runs; run++) {
        trial_seeds.emplace_back(run, seed_generator_->next());
    }

    // Process trials in parallel batches
    std::vector<std::jthread> workers;
    workers.reserve(config_.concurrent_trials);

    // Start progress monitoring
    std::jthread monitor_thread;
    if (config_.enable_monitoring) {
        monitor_thread = std::jthread([this]() { monitor_progress(); });
    }

    // Split trials into batches for concurrent execution
    size_t trials_per_batch = std::max(1u, trial_runs / config_.concurrent_trials);
    size_t remaining_trials = trial_runs;

    for (size_t batch_start = 0; batch_start < trial_runs && !source_.stop_requested();
         batch_start += trials_per_batch) {

        size_t batch_size = std::min(trials_per_batch, remaining_trials);
        std::vector<std::pair<unsigned int, unsigned int>> batch_seeds(
            trial_seeds.begin() + batch_start, trial_seeds.begin() + batch_start + batch_size);

        workers.emplace_back(&ParallelRunner::run_trial_batch, this, source_.get_token(),
                             std::ref(baseline), std::move(batch_seeds));

        remaining_trials -= batch_size;
    }

    // Wait for all workers to complete
    for (auto &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    // Stop monitoring
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }

    ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = elapsed.count();

    // Final performance report
    if (config_.enable_monitoring) {
        unsigned int completed = metrics_.completed_trials.load();
        double avg_time = metrics_.avg_trial_time_ms.load();
        double throughput = (completed > 0) ? (completed * 1000.0 / elapsed_ms) : 0.0;

        fmt::print(fmt::fg(fmt::color::light_green),
                   "\n✅ COMPLETED: {} trials in {:.2f}s | Avg: {:.1f}ms/trial | Throughput: "
                   "{:.2f} trials/sec\n",
                   completed, elapsed_ms / 1000.0, avg_time, throughput);
    }

    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::finish, elapsed_ms));
    return elapsed_ms;
}

double ParallelRunner::run(Simulation &baseline, Simulation &intervention,
                           unsigned int trial_runs) {
    if (trial_runs < 1) {
        throw std::invalid_argument("The number of trial runs must not be less than one.");
    }
    if (baseline.type() != ScenarioType::baseline) {
        throw std::invalid_argument(
            fmt::format("Baseline simulation: {} type mismatch.", baseline.name()));
    }
    if (intervention.type() != ScenarioType::intervention) {
        throw std::invalid_argument(
            fmt::format("Intervention simulation: {} type mismatch.", intervention.name()));
    }
    if (running_.load()) {
        throw std::invalid_argument("The parallel runner is already evaluating an experiment.");
    }

    running_.store(true);
    auto reset = make_finally([this]() {
        running_.store(false);
        cleanup_trial_threading();
    });

    runner_id_ = "optimized_parallel_runner";
    setup_trial_threading();

    // Initialize performance metrics
    metrics_.start_time = std::chrono::steady_clock::now();
    metrics_.last_update = metrics_.start_time;
    metrics_.completed_trials = 0;
    metrics_.total_trials = trial_runs * 2; // Both baseline and intervention
    metrics_.trial_times.clear();
    metrics_.trial_times.reserve(trial_runs * 2);

    auto start = std::chrono::steady_clock::now();
    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::start));

    fmt::print(fmt::fg(fmt::color::yellow),
               "\n🔥 Starting OPTIMIZED baseline + intervention: {} trial pairs with {} concurrent "
               "workers\n",
               trial_runs, config_.concurrent_trials);

    // Generate all trial seeds upfront
    std::vector<std::pair<unsigned int, unsigned int>> trial_seeds;
    trial_seeds.reserve(trial_runs);
    for (auto run = 1u; run <= trial_runs; run++) {
        trial_seeds.emplace_back(run, seed_generator_->next());
    }

    // Start progress monitoring
    std::jthread monitor_thread;
    if (config_.enable_monitoring) {
        monitor_thread = std::jthread([this]() { monitor_progress(); });
    }

    // Use TBB task group for optimal parallel execution
    tbb::task_group task_group;

    for (const auto &[run_number, seed] : trial_seeds) {
        if (source_.stop_requested())
            break;

        // Launch baseline and intervention trials concurrently
        task_group.run([this, &baseline, run_number, seed]() {
            run_trial_worker(source_.get_token(), baseline, run_number, seed);
        });

        task_group.run([this, &intervention, run_number, seed]() {
            run_trial_worker(source_.get_token(), intervention, run_number, seed);
        });
    }

    // Wait for all tasks to complete
    task_group.wait();

    // Stop monitoring
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }

    ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = elapsed.count();

    // Final performance report
    if (config_.enable_monitoring) {
        unsigned int completed = metrics_.completed_trials.load();
        double avg_time = metrics_.avg_trial_time_ms.load();
        double throughput = (completed > 0) ? (completed * 1000.0 / elapsed_ms) : 0.0;

        fmt::print(fmt::fg(fmt::color::light_green),
                   "\n✅ COMPLETED: {} total simulations in {:.2f}s | Avg: {:.1f}ms/trial | "
                   "Throughput: {:.2f} trials/sec\n",
                   completed, elapsed_ms / 1000.0, avg_time, throughput);
    }

    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::finish, elapsed_ms));
    return elapsed_ms;
}

bool ParallelRunner::is_running() const noexcept { return running_.load(); }

void ParallelRunner::cancel() noexcept {
    if (is_running()) {
        source_.request_stop();
    }
}

const PerformanceMetrics &ParallelRunner::get_metrics() const noexcept { return metrics_; }

void ParallelRunner::run_trial_batch(
    const std::stop_token &token, Simulation &simulation,
    const std::vector<std::pair<unsigned int, unsigned int>> &trial_seeds) {
    for (const auto &[run_number, seed] : trial_seeds) {
        if (token.stop_requested())
            break;
        run_trial_worker(token, simulation, run_number, seed);
    }
}

void ParallelRunner::run_trial_worker(const std::stop_token &token, Simulation &simulation,
                                      unsigned int run_number, unsigned int seed) {
    auto run_start = std::chrono::steady_clock::now();
    notify(
        std::make_unique<RunnerEventMessage>(fmt::format("{} - {}", runner_id_, simulation.name()),
                                             RunnerAction::run_begin, run_number));

    /* Create the simulation engine */
    adevs::Simulator<int> sim;

    /* Update model and add to engine */
    simulation.setup_run(run_number, seed);
    sim.add(&simulation);

    /* Run until the next event is at infinity */
    while (!token.stop_requested() && sim.next_event_time() < adevs_inf<adevs::Time>()) {
        sim.exec_next_event();
    }

    ElapsedTime elapsed = std::chrono::steady_clock::now() - run_start;
    auto elapsed_ms = elapsed.count();

    // Update performance metrics
    metrics_.update_completion(elapsed_ms);

    notify(
        std::make_unique<RunnerEventMessage>(fmt::format("{} - {}", runner_id_, simulation.name()),
                                             RunnerAction::run_end, run_number, elapsed_ms));
}

void ParallelRunner::monitor_progress() {
    while (running_.load() && !source_.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::duration<double>(config_.progress_interval));
        if (config_.enable_monitoring) {
            metrics_.print_progress();
        }
    }
}

void ParallelRunner::setup_trial_threading() {
    // Optimize TBB thread allocation for individual trials
    if (config_.threads_per_trial > 0) {
        trial_thread_control_ = std::make_unique<tbb::global_control>(
            tbb::global_control::max_allowed_parallelism, config_.threads_per_trial);
    }
}

void ParallelRunner::cleanup_trial_threading() { trial_thread_control_.reset(); }

void ParallelRunner::notify(std::unique_ptr<EventMessage> message) {
    if (event_bus_) {
        event_bus_->publish(std::move(message));
    }
}

} // namespace hgps
