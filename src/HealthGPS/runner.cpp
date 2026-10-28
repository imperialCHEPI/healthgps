#include "runner.h"
#include "failure_report.h"
#include "finally.h"
#include "runner_message.h"

#include <chrono>
#include <fmt/format.h>
#include <thread>

namespace hgps {

using ElapsedTime = std::chrono::duration<double, std::milli>;

// MAHIMA: Read both worker errors after join. exception_ptr has no message until it is rethrown.
void rethrow_worker_failures(const std::exception_ptr &baseline_error,
                             const std::exception_ptr &intervention_error) {
    if (!baseline_error && !intervention_error) {
        return;
    }
    if (baseline_error && !intervention_error) {
        std::rethrow_exception(baseline_error);
    }
    if (!baseline_error && intervention_error) {
        std::rethrow_exception(intervention_error);
    }

    std::string baseline_text;
    std::string intervention_text;
    try {
        std::rethrow_exception(baseline_error);
    } catch (const std::exception &ex) {
        baseline_text = ex.what();
    }
    try {
        std::rethrow_exception(intervention_error);
    } catch (const std::exception &ex) {
        intervention_text = ex.what();
    }
    throw std::runtime_error(baseline_text + "\n\nSecond scenario also failed:\n" +
                             intervention_text);
}

Runner::Runner(std::shared_ptr<EventAggregator> bus,
               std::unique_ptr<RandomBitGenerator> seed_generator) noexcept
    : running_{false}, event_bus_{std::move(bus)}, seed_generator_{std::move(seed_generator)} {}

double Runner::run(Simulation &baseline, const unsigned int trial_runs) {
    if (trial_runs < 1) {
        throw std::invalid_argument("The number of trial runs must not be less than one");
    }
    if (baseline.type() != ScenarioType::baseline) {
        throw std::invalid_argument(
            fmt::format("Simulation: '{}' cannot be evaluated alone", baseline.name()));
    }

    if (running_.load()) {
        throw std::invalid_argument("The model runner is already evaluating an experiment");
    }

    source_ = std::stop_source{};
    running_.store(true);
    auto reset = make_finally([this]() { running_.store(false); });

    runner_id_ = "single_runner";
    auto start = std::chrono::steady_clock::now();
    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::start));

    for (auto run = 1u; run <= trial_runs; run++) {
        unsigned int run_seed = seed_generator_->next();

        std::exception_ptr error;
        auto worker = std::jthread(&Runner::run_model_thread, this, source_.get_token(),
                                   std::ref(baseline), run, run_seed, std::ref(error));

        worker.join();
        // MAHIMA: The worker stores the exception. Rethrow here so main can print it.
        // Leaving the exception on the jthread calls std::terminate with no message.
        if (error) {
            std::rethrow_exception(error);
        }
        if (source_.stop_requested()) {
            notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::cancelled));
            break;
        }
    }

    ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = elapsed.count();
    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::finish, elapsed_ms));
    running_.store(false);
    return elapsed_ms;
}

double Runner::run(Simulation &baseline, Simulation &intervention, const unsigned int trial_runs) {
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
        throw std::invalid_argument("The model runner is already evaluating an experiment.");
    }

    running_.store(true);
    auto reset = make_finally([this]() { running_.store(false); });

    runner_id_ = "parallel_runner";
    auto start = std::chrono::steady_clock::now();
    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::start));

    for (auto run = 1u; run <= trial_runs; run++) {
        unsigned int run_seed = seed_generator_->next();

        std::exception_ptr baseline_error;
        std::exception_ptr intervention_error;
        auto base_worker = std::jthread(&Runner::run_model_thread, this, source_.get_token(),
                                        std::ref(baseline), run, run_seed, std::ref(baseline_error));

        auto policy_worker =
            std::jthread(&Runner::run_model_thread, this, source_.get_token(),
                         std::ref(intervention), run, run_seed, std::ref(intervention_error));

        base_worker.join();
        policy_worker.join();
        rethrow_worker_failures(baseline_error, intervention_error);
        if (source_.stop_requested()) {
            notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::cancelled));
            break;
        }
    }

    ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = elapsed.count();

    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::finish, elapsed_ms));
    return elapsed_ms;
}

bool Runner::is_running() const noexcept { return running_.load(); }

void Runner::cancel() noexcept {
    if (is_running()) {
        source_.request_stop();
    }
}

void Runner::run_model_thread(const std::stop_token &token, Simulation &model, unsigned int run,
                              const unsigned int seed, std::exception_ptr &error) {
    // MAHIMA: Catch inside the thread. std::jthread calls std::terminate if this function throws,
    // and that exit used to drop the exception text (the Slovenia run died after disease init).
    try {
        SimulationPhase phase{fmt::format("scenario '{}' run {}", model.name(), run)};
        auto run_start = std::chrono::steady_clock::now();
        notify(std::make_unique<RunnerEventMessage>(
            fmt::format("{} - {}", runner_id_, model.name()), RunnerAction::run_begin, run));

        adevs::Simulator<int> sim;
        model.setup_run(run, seed);
        sim.add(&model);

        while (!token.stop_requested() && sim.next_event_time() < adevs_inf<adevs::Time>()) {
            sim.exec_next_event();
        }

        ElapsedTime elapsed = std::chrono::steady_clock::now() - run_start;
        notify(std::make_unique<RunnerEventMessage>(
            fmt::format("{} - {}", runner_id_, model.name()), RunnerAction::run_end, run,
            elapsed.count()));
    } catch (const std::exception &ex) {
        error = std::make_exception_ptr(
            std::runtime_error(format_failure(current_simulation_phase(), ex)));
        clear_simulation_phase();
    } catch (...) {
        error = std::make_exception_ptr(
            std::runtime_error(format_unknown_failure(current_simulation_phase())));
        clear_simulation_phase();
    }
}

void Runner::notify(std::unique_ptr<hgps::EventMessage> message) {
    event_bus_->publish_async(std::move(message));
}

} // namespace hgps
