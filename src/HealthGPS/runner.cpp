#include "runner.h"
#include "finally.h"
#include "runner_message.h"

#include <chrono>
#include <fmt/format.h>
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
    while (!token.stop_requested() && sim.next_event_time() < adevs_inf<adevs::Time>()) {
        sim.exec_next_event();
    }

    ElapsedTime elapsed = std::chrono::steady_clock::now() - run_start;
    notify(std::make_unique<RunnerEventMessage>(fmt::format("{} - {}", runner_id_, model.name()),
                                                RunnerAction::run_end, run, elapsed.count()));
}

void Runner::notify(std::unique_ptr<hgps::EventMessage> message) {
    event_bus_->publish_async(std::move(message));
}

} // namespace hgps
