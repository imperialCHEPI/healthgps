#include "modelrunner.h"
#include "finally.h"
#include "runner_message.h"

#include <chrono>
#include <fmt/format.h>
#include <thread>

namespace hgps {

using ElapsedTime = std::chrono::duration<double, std::milli>;

ModelRunner::ModelRunner(EventAggregator &bus,
                         std::unique_ptr<RandomBitGenerator> seed_generator) noexcept
    : running_{false}, event_bus_{bus}, seed_generator_{std::move(seed_generator)} {}

double ModelRunner::run(HealthGPS &baseline, const unsigned int trial_runs) {
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

    /* Initialise simulation */
    baseline.initialize();

    for (auto run = 1u; run <= trial_runs; run++) {
        auto run_seed = std::optional<unsigned int>{seed_generator_->operator()()};

        auto worker = std::jthread(&ModelRunner::run_model_thread, this, source_.get_token(),
                                   std::ref(baseline), run, run_seed);

        worker.join();
        if (source_.stop_requested()) {
            notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::cancelled));
            break;
        }
    }

    /* Terminate simulation */
    baseline.terminate();

    ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = elapsed.count();
    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::finish, elapsed_ms));
    running_.store(false);
    return elapsed_ms;
}

double ModelRunner::run(HealthGPS &baseline, HealthGPS &intervention,
                        const unsigned int trial_runs) {
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

    /* Initialise simulation */
    baseline.initialize();
    intervention.initialize();

    for (auto run = 1u; run <= trial_runs; run++) {
        auto run_seed = std::optional<unsigned int>{seed_generator_->operator()()};

        auto base_worker = std::jthread(&ModelRunner::run_model_thread, this, source_.get_token(),
                                        std::ref(baseline), run, run_seed);

        auto policy_worker = std::jthread(&ModelRunner::run_model_thread, this, source_.get_token(),
                                          std::ref(intervention), run, run_seed);

        base_worker.join();
        policy_worker.join();
        if (source_.stop_requested()) {
            notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::cancelled));
            break;
        }
    }

    /* Terminate simulation */
    baseline.terminate();
    intervention.terminate();

    ElapsedTime elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = elapsed.count();

    notify(std::make_unique<RunnerEventMessage>(runner_id_, RunnerAction::finish, elapsed_ms));
    return elapsed_ms;
}

bool ModelRunner::is_running() const noexcept { return running_.load(); }

void ModelRunner::cancel() noexcept {
    if (is_running()) {
        source_.request_stop();
    }
}

void ModelRunner::run_model_thread(const std::stop_token &token, HealthGPS &model, unsigned int run,
                                   const std::optional<unsigned int> seed) {
    auto run_start = std::chrono::steady_clock::now();
    notify(std::make_unique<RunnerEventMessage>(fmt::format("{} - {}", runner_id_, model.name()),
                                                RunnerAction::run_begin, run));

    /* Create the simulation engine */
    adevs::Simulator<int> sim;

    /* Update model and add to engine */
    if (seed.has_value()) {
        model.setup_run(run, seed.value());
    } else {
        model.setup_run(run);
    }

    sim.add(&model);

    /* Run until the next event is at infinity */
    while (!token.stop_requested() && sim.next_event_time() < adevs_inf<adevs::Time>()) {
        sim.exec_next_event();
    }

    ElapsedTime elapsed = std::chrono::steady_clock::now() - run_start;
    notify(std::make_unique<RunnerEventMessage>(fmt::format("{} - {}", runner_id_, model.name()),
                                                RunnerAction::run_end, run, elapsed.count()));
}

void ModelRunner::notify(std::unique_ptr<hgps::EventMessage> message) {
    event_bus_.get().publish_async(std::move(message));
}
} // namespace hgps
