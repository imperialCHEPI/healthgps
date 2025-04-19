#include "simulation.h"
#include "HealthGPS.Core/thread_util.h"
#include "HealthGPS.Core/univariate_summary.h"
#include "converter.h"
#include "finally.h"
#include "info_message.h"
#include "mtrandom.h"
#include "sync_message.h"
#include "univariate_visitor.h"

#include <algorithm>
#include <fmt/format.h>
#include <iostream>
#include <memory>
#include <oneapi/tbb/parallel_for_each.h>
#include <stdexcept>

namespace { // anonymous namespace

/// @brief Defines the net immigration synchronisation message
using NetImmigrationMessage = hgps::SyncDataMessage<hgps::IntegerAgeGenderTable>;

} // anonymous namespace

namespace hgps {

Simulation::Simulation(SimulationModuleFactory &factory, std::shared_ptr<const EventAggregator> bus,
                       std::shared_ptr<const ModelInput> inputs, std::unique_ptr<Scenario> scenario)
    : context_{std::move(bus), std::move(inputs), std::move(scenario)} {

    // Create required modules, should change to shared_ptr
    auto ses_base = factory.create(SimulationModuleType::SES, context_.inputs());
    auto dem_base = factory.create(SimulationModuleType::Demographic, context_.inputs());
    auto risk_base = factory.create(SimulationModuleType::RiskFactor, context_.inputs());
    auto disease_base = factory.create(SimulationModuleType::Disease, context_.inputs());
    auto analysis_base = factory.create(SimulationModuleType::Analysis, context_.inputs());

    ses_ = std::static_pointer_cast<UpdatableModule>(ses_base);
    demographic_ = std::static_pointer_cast<DemographicModule>(dem_base);
    risk_factor_ = std::static_pointer_cast<RiskFactorHostModule>(risk_base);
    disease_ = std::static_pointer_cast<DiseaseModule>(disease_base);
    analysis_ = std::static_pointer_cast<UpdatableModule>(analysis_base);
}

void Simulation::setup_run(unsigned int run_number, unsigned int run_seed) noexcept {
    // std::cout << "\nDEBUG: Simulation::setup_run - Starting for run #" << run_number << " with
    // seed " << run_seed << std::endl;
    context_.set_current_run(run_number);
    context_.random().seed(run_seed);
    // std::cout << "\nDEBUG: Simulation::setup_run - Completed" << std::endl;
}

adevs::Time Simulation::init(adevs::SimEnv<int> *env) {
    // std::cout << "\nDEBUG: Simulation::init - Starting" << std::endl;
    auto start = std::chrono::steady_clock::now();
    const auto &inputs = context_.inputs();
    auto world_time = inputs.start_time();
    context_.metrics().clear();
    context_.scenario().clear();
    context_.set_current_time(world_time);
    end_time_ = adevs::Time(inputs.stop_time(), 0);
    // std::cout << "\nDEBUG: Simulation::init - Initializing population" << std::endl;
    initialise_population();
    // std::cout << "\nDEBUG: Simulation::init - Population initialized" << std::endl;

    auto stop = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(stop - start);

    auto message =
        fmt::format("[{:4},{}] population size: {}, elapsed: {}ms", env->now().real,
                    env->now().logical, context_.population().initial_size(), elapsed.count());
    context_.publish(std::make_unique<InfoEventMessage>(
        name(), ModelAction::start, context_.current_run(), context_.time_now(), message));

    std::cout << "\nDEBUG: Simulation::init - Completed, next time: " << world_time << std::endl;
    return env->now() + adevs::Time(world_time, 0);
}

adevs::Time Simulation::update(adevs::SimEnv<int> *env) {
    // std::cout << "\nDEBUG: Simulation::update - Starting at time " << env->now().real <<
    // std::endl;
    if (env->now() < end_time_) {
        auto start = std::chrono::steady_clock::now();
        context_.metrics().reset();

        // Now move world clock to time t + 1
        auto world_time = env->now() + adevs::Time(1, 0);
        auto time_year = world_time.real;
        context_.set_current_time(time_year);
        std::cout << "\nDEBUG: Simulation::update - Updating population for time " << time_year
                  << std::endl;
        update_population();
        std::cout << "\nDEBUG: Simulation::update - Population updated" << std::endl;

        auto stop = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double, std::milli>(stop - start);

        auto message = fmt::format("[{:4},{}], elapsed: {}ms", env->now().real, env->now().logical,
                                   elapsed.count());
        context_.publish(std::make_unique<InfoEventMessage>(
            name(), ModelAction::update, context_.current_run(), context_.time_now(), message));

        // Schedule next event time
        std::cout << "\nDEBUG: Simulation::update - Completed, next time: " << world_time.real
                  << std::endl;
        return world_time;
    }

    // We have reached the end, remove the model and return infinite time for next event.
    std::cout << "\nDEBUG: Simulation::update - End time reached, removing model" << std::endl;
    env->remove(this);
    return adevs_inf<adevs::Time>();
}

adevs::Time Simulation::update(adevs::SimEnv<int> * /*env*/, std::vector<int> & /*x*/) {
    // This method is never called because nobody sends messages.
    return adevs_inf<adevs::Time>();
}

void Simulation::fini(adevs::Time clock) {
    // risk_factor_->update_population(context_);
    auto message = fmt::format("[{:4},{}] clear up resources.", clock.real, clock.logical);
    context_.publish(std::make_unique<InfoEventMessage>(
        name(), ModelAction::stop, context_.current_run(), context_.time_now(), message));
}

void Simulation::initialise_population() {
    // std::cout << "\nDEBUG: Simulation::initialise_population - Starting" << std::endl;
    /* Note: order is very important */

    // Create virtual population
    const auto &inputs = context_.inputs();
    auto model_start_year = inputs.start_time();
    auto total_year_pop_size = demographic_->get_total_population_size(model_start_year);
    float size_fraction = inputs.settings().size_fraction();
    auto virtual_pop_size = static_cast<int>(size_fraction * total_year_pop_size);
    context_.reset_population(virtual_pop_size);
    std::cout << "\nDEBUG: population with size " << virtual_pop_size << std::endl;

    // Gender - Age, must be first
    std::cout << "\nDEBUG: Simulation::initialise_population - Initializing demographic";
    demographic_->initialise_population(context_);
    std::cout << "\nDEBUG: Simulation::initialise_population - Demographic completed";

    // Social economics status- NOT BEING USED FOR FINCH- Mahima
    /*std::cout << "\nDEBUG: Simulation::initialise_population - Initializing SES" << std::endl;
    ses_->initialise_population(context_);
    std::cout << "\nDEBUG: Simulation::initialise_population - SES completed" << std::endl;*/

    // Generate risk factors
    std::cout << "\nDEBUG: Simulation::initialise_population - Initializing risk factors"
              << std::endl;
    risk_factor_->initialise_population(context_);
    std::cout << "\nDEBUG: Simulation::initialise_population - Risk factors completed";

    // Initialise diseases
    std::cout << "\nDEBUG: Simulation::initialise_population - Initializing diseases";
    disease_->initialise_population(context_);
    std::cout << "\nDEBUG: Simulation::initialise_population - Diseases completed";

    // Initialise analysis
    std::cout << "\nDEBUG: Simulation::initialise_population - Initializing analysis";
    analysis_->initialise_population(context_);
    std::cout << "\nDEBUG: Simulation::initialise_population - Analysis completed";

    print_initial_population_statistics();
    std::cout << "\nDEBUG: Simulation::initialise_population - Completed";
}

void Simulation::update_population() {
    std::cout << "\nDEBUG: Simulation::update_population - Starting";
    /* Note: order is very important */

    // update basic information: demographics + diseases
    std::cout << "\nDEBUG: Simulation::update_population - Updating demographic";
    demographic_->update_population(context_, *disease_);
    std::cout << "\nDEBUG: Simulation::update_population - Demographic updated";

    // Calculate the net immigration by gender and age, update the population accordingly
    std::cout << "\nDEBUG: Simulation::update_population - Updating net immigration";
    update_net_immigration();
    std::cout << "\nDEBUG: Simulation::update_population - Net immigration updated";

    // update population socio-economic status- Not using SES for FINCH- Mahima
    /*std::cout << "\nDEBUG: Simulation::update_population - Updating SES" << std::endl;
    ses_->update_population(context_);
    std::cout << "\nDEBUG: Simulation::update_population - SES updated" << std::endl;*/

    // Update population risk factors
    std::cout << "\nDEBUG: Simulation::update_population - Updating risk factors";
    risk_factor_->update_population(context_);
    std::cout << "\nDEBUG: Simulation::update_population - Risk factors updated";

    // Update diseases status: remission and incidence
    std::cout << "\nDEBUG: Simulation::update_population - Updating diseases";
    disease_->update_population(context_);
    std::cout << "\nDEBUG: Simulation::update_population - Diseases updated";

    // Publish results to data logger
    std::cout << "\nDEBUG: Simulation::update_population - Updating analysis";
    analysis_->update_population(context_);
    std::cout << "\nDEBUG: Simulation::update_population - Analysis updated";

    std::cout << "\nDEBUG: Simulation::update_population - Completed";
}

void Simulation::update_net_immigration() {
    auto net_immigration = get_net_migration();

    // Update population based on net immigration
    auto start_age = context_.age_range().lower();
    auto end_age = context_.age_range().upper();
    for (int age = start_age; age <= end_age; age++) {
        auto male_net_value = net_immigration.at(age, core::Gender::male);
        apply_net_migration(male_net_value, age, core::Gender::male);

        auto female_net_value = net_immigration.at(age, core::Gender::female);
        apply_net_migration(female_net_value, age, core::Gender::female);
    }

    if (context_.scenario().type() == ScenarioType::baseline) {
        context_.scenario().channel().send(std::make_unique<NetImmigrationMessage>(
            context_.current_run(), context_.time_now(), std::move(net_immigration)));
    }
}

IntegerAgeGenderTable Simulation::get_current_expected_population() const {
    const auto &inputs = context_.inputs();
    auto sim_start_time = context_.start_time();
    auto total_initial_population = demographic_->get_total_population_size(sim_start_time);
    float size_fraction = inputs.settings().size_fraction();
    auto start_population_size = static_cast<int>(size_fraction * total_initial_population);

    const auto &current_population_table =
        demographic_->get_population_distribution(context_.time_now());
    auto expected_population = create_age_gender_table<int>(context_.age_range());
    auto start_age = context_.age_range().lower();
    auto end_age = context_.age_range().upper();
    for (int age = start_age; age <= end_age; age++) {
        const auto &age_info = current_population_table.at(age);
        expected_population.at(age, core::Gender::male) = static_cast<int>(
            std::round(age_info.males * start_population_size / total_initial_population));

        expected_population.at(age, core::Gender::female) = static_cast<int>(
            std::round(age_info.females * start_population_size / total_initial_population));
    }

    return expected_population;
}

IntegerAgeGenderTable Simulation::get_current_simulated_population() {
    auto simulated_population = create_age_gender_table<int>(context_.age_range());
    auto &pop = context_.population();
    auto count_mutex = std::mutex{};
    tbb::parallel_for_each(pop.cbegin(), pop.cend(), [&](const auto &entity) {
        if (!entity.is_active()) {
            return;
        }

        auto lock = std::unique_lock{count_mutex};
        simulated_population.at(entity.age, entity.gender)++;
    });

    return simulated_population;
}

void Simulation::apply_net_migration(int net_value, unsigned int age, const core::Gender &gender) {
    if (net_value > 0) {
        auto &pop = context_.population();
        auto similar_indices = core::find_index_of_all(pop, [&](const Person &entity) {
            return entity.is_active() && entity.age == age && entity.gender == gender;
        });

        if (!similar_indices.empty()) {
            // Needed for repeatability in random selection
            std::sort(similar_indices.begin(), similar_indices.end());

            for (auto trial = 0; trial < net_value; trial++) {
                auto index =
                    context_.random().next_int(static_cast<int>(similar_indices.size()) - 1);
                const auto &source = pop.at(similar_indices.at(index));
                context_.population().add(partial_clone_entity(source), context_.time_now());
            }
        }
    } else if (net_value < 0) {
        auto net_value_counter = net_value;
        for (auto &entity : context_.population()) {
            if (!entity.is_active()) {
                continue;
            }

            if (entity.age == age && entity.gender == gender) {
                entity.emigrate(context_.time_now());
                net_value_counter++;
                if (net_value_counter == 0) {
                    break;
                }
            }
        }
    }
}

hgps::IntegerAgeGenderTable Simulation::get_net_migration() {
    if (context_.scenario().type() == ScenarioType::baseline) {
        return create_net_migration();
    }

    // Receive message with timeout
    auto message = context_.scenario().channel().try_receive(context_.sync_timeout_millis());
    if (message.has_value()) {
        auto &basePtr = message.value();
        auto *messagePrt = dynamic_cast<NetImmigrationMessage *>(basePtr.get());
        if (messagePrt) {
            return messagePrt->data();
        }

        throw std::runtime_error(
            "Simulation out of sync, failed to receive a net immigration message");
    }
    throw std::runtime_error(fmt::format(
        "Simulation out of sync, receive net immigration message has timed out after {} ms.",
        context_.sync_timeout_millis()));
}

hgps::IntegerAgeGenderTable Simulation::create_net_migration() {
    auto expected_future = core::run_async(&Simulation::get_current_expected_population, this);
    auto simulated_population = get_current_simulated_population();
    auto net_emigration = create_age_gender_table<int>(context_.age_range());
    auto start_age = context_.age_range().lower();
    auto end_age = context_.age_range().upper();
    auto expected_population = expected_future.get();
    auto net_value = 0;
    for (int age = start_age; age <= end_age; age++) {
        net_value = expected_population.at(age, core::Gender::male) -
                    simulated_population.at(age, core::Gender::male);
        net_emigration.at(age, core::Gender::male) = net_value;

        net_value = expected_population.at(age, core::Gender::female) -
                    simulated_population.at(age, core::Gender::female);
        net_emigration.at(age, core::Gender::female) = net_value;
    }

    // Update statistics
    return net_emigration;
}

Person Simulation::partial_clone_entity(const Person &source) noexcept {
    auto clone = Person{};
    clone.age = source.age;
    clone.gender = source.gender;
    clone.ses = source.ses;
    clone.sector = source.sector;
    clone.region = source.region; // added for FINCH
    clone.ethnicity = source.ethnicity;
    clone.income = source.income;
    clone.income_continuous = source.income_continuous;
    clone.income_category = source.income_category;
    clone.physical_activity = source.physical_activity;
    for (const auto &item : source.risk_factors) {
        clone.risk_factors.emplace(item.first, item.second);
    }

    for (const auto &item : source.diseases) {
        clone.diseases.emplace(item.first, item.second.clone());
    }

    return clone;
}

std::map<std::string, core::UnivariateSummary> Simulation::create_input_data_summary() const {
    auto visitor = UnivariateVisitor();
    auto summary = std::map<std::string, core::UnivariateSummary>();
    const auto &input_data = context_.inputs().data();

    for (const auto &entry : context_.mapping()) {
        // HACK: Ignore missing columns.
        if (const auto &column = input_data.column_if_exists(entry.name())) {
            column->get().accept(visitor);
            summary.emplace(entry.name(), visitor.get_summary());
        }
    }

    return summary;
}

void hgps::Simulation::print_initial_population_statistics() {
    auto verbosity = context_.inputs().run().verbosity;
    if (context_.current_run() > 1 && verbosity == core::VerboseMode::none) {
        return;
    }

    auto original_future = core::run_async(&Simulation::create_input_data_summary, this);
    std::string population = "Population";
    std::size_t longestColumnName = population.length();
    auto sim_summary = std::map<std::string, core::UnivariateSummary>();
    for (const auto &entry : context_.mapping()) {
        longestColumnName = std::max(longestColumnName, entry.name().length());
        sim_summary.emplace(entry.name(), core::UnivariateSummary(entry.name()));
    }

    for (const auto &entity : context_.population()) {
        for (const auto &entry : context_.mapping()) {
            sim_summary[entry.name()].append(entity.get_risk_factor_value(entry.key()));
        }
    }

    auto pad = longestColumnName + 2;
    auto width = pad + 70;
    auto orig_pop = context_.inputs().data().num_rows();
    auto sim_pop = context_.population().size();

    std::stringstream ss;
    ss << fmt::format("\n Initial Virtual Population Summary: {}\n", context_.identifier());
    ss << fmt::format("|{:-<{}}|\n", '-', width);
    ss << fmt::format("| {:{}} : {:>14} : {:>14} : {:>14} : {:>14} |\n", "Variable", pad,
                      "Mean (Real)", "Mean (Sim)", "StdDev (Real)", "StdDev (Sim)");
    ss << fmt::format("|{:-<{}}|\n", '-', width);

    ss << fmt::format("| {:{}} : {:14} : {:14} : {:14} : {:14} |\n", population, pad, orig_pop,
                      sim_pop, orig_pop, sim_pop);

    auto orig_summary = original_future.get();
    for (const auto &entry : context_.mapping()) {
        const auto &col = entry.name();
        ss << fmt::format("| {:{}} : {:14.4f} : {:14.5f} : {:14.5f} : {:14.5f} |\n", col, pad,
                          orig_summary[col].average(), sim_summary[col].average(),
                          orig_summary[col].std_deviation(), sim_summary[col].std_deviation());
    }

    ss << fmt::format("|{:_<{}}|\n\n", '_', width);
    std::cout << ss.str();
}

} // namespace hgps
