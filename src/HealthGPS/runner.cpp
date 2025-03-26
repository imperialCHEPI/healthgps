#include "runner.h"
#include "finally.h"
#include "runner_message.h"

#include <chrono>
#include <fmt/format.h>
#include <iostream>
#include <thread>

namespace hgps {

using ElapsedTime = std::chrono::duration<double, std::milli>;

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

        auto worker = std::jthread(&Runner::run_model_thread, this, source_.get_token(),
                                   std::ref(baseline), run, run_seed);

        worker.join();
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

        auto base_worker = std::jthread(&Runner::run_model_thread, this, source_.get_token(),
                                        std::ref(baseline), run, run_seed);

        auto policy_worker = std::jthread(&Runner::run_model_thread, this, source_.get_token(),
                                          std::ref(intervention), run, run_seed);

        base_worker.join();
        policy_worker.join();
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
                              const unsigned int seed) {
    auto run_start = std::chrono::steady_clock::now();
    notify(std::make_unique<RunnerEventMessage>(fmt::format("{} - {}", runner_id_, model.name()),
                                                RunnerAction::run_begin, run));

    /* Create the simulation engine */
    adevs::Simulator<int> sim;

    /* Update model and add to engine */
    model.setup_run(run, seed);
    sim.add(&model);

    /* Run until the next event is at infinity */
    int step_count = 0;
    int max_steps = 100; // Safety limit to prevent infinite loops
    std::cout << "\n********** STARTING " << model.name() << " SIMULATION RUN #" << run
              << " **********" << std::endl;

    // Get initial next event time
    auto next_time = sim.next_event_time();

    while (!token.stop_requested() && sim.next_event_time() < adevs_inf<adevs::Time>() &&
           step_count < max_steps) {

        next_time = sim.next_event_time();

        // Execute the next event
        sim.exec_next_event();
        step_count++;

        // Get next event time after execution to check if we need to continue
        auto new_next_time = sim.next_event_time();

        // Verify that the next event time is changing
        if (!(new_next_time < adevs_inf<adevs::Time>())) {
            std::cout << "\n********** " << model.name() << " SIMULATION COMPLETE AFTER "
                      << step_count << " STEPS **********" << std::endl;
            break;
        }

        // Safety check: if we're stuck at the same time, break out
        if (step_count > 1 && new_next_time.real == next_time.real) {
            std::cout << "WARNING: [" << model.name()
                      << "] Event time not advancing, possible infinite loop. Breaking out."
                      << std::endl;
            break;
        }
    }

    // Show final reason for exiting loop
    if (token.stop_requested()) {
        std::cout << "SIMULATION CANCELLED: User requested stop after " << step_count << " steps"
                  << std::endl;
    } else if (!(sim.next_event_time() < adevs_inf<adevs::Time>())) {
        // This is the normal completion case - already printed above
    } else if (step_count >= max_steps) {
        std::cout << "WARNING: " << model.name() << " reached maximum step count (" << max_steps
                  << ")" << std::endl;
    }

    ElapsedTime elapsed = std::chrono::steady_clock::now() - run_start;
    notify(std::make_unique<RunnerEventMessage>(fmt::format("{} - {}", runner_id_, model.name()),
                                                RunnerAction::run_end, run, elapsed.count()));
}

void Runner::notify(std::unique_ptr<hgps::EventMessage> message) {
    event_bus_->publish_async(std::move(message));
}

} // namespace hgps
