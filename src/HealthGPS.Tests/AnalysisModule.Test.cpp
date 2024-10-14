#include "pch.h"

#include "HealthGPS/analysis_module.h"

#include "simulation.h"

hgps::Person create_test_person(int age = 20,
                                hgps::core::Gender gender = hgps::core::Gender::male) {
    auto person = hgps::Person{};
    person.age = age;
    person.gender = gender;
    return person;
}

namespace hgps {
class TestAnalysisModule : public ::testing::Test {
  protected:
    hgps::core::DataTable data;

    hgps::input::DataManager manager = hgps::input::DataManager(test_datastore_path);
    hgps::CachedRepository repository = hgps::CachedRepository(manager);

    hgps::ModelInput inputs = create_test_configuration(data);

    std::unique_ptr<hgps::AnalysisModule> analysis_module =
        build_analysis_module(repository, inputs);

    hgps::Person test_person_1 = create_test_person(16, hgps::core::Gender::male);
    hgps::Person test_person_2 = create_test_person(19, hgps::core::Gender::male);
    hgps::DefaultEventBus bus = DefaultEventBus{};
    hgps::SyncChannel channel;
    std::unique_ptr<hgps::MTRandom32> rnd = std::make_unique<MTRandom32>(123456789);
    std::unique_ptr<hgps::BaselineScenario> scenario = std::make_unique<BaselineScenario>(channel);
    hgps::SimulationDefinition definition =
        SimulationDefinition(inputs, std::move(scenario), std::move(rnd));

    hgps::RuntimeContext context = RuntimeContext(bus, definition);

    TestAnalysisModule() {
        create_test_datatable(data);

        auto config = create_test_configuration(data);

        context.set_current_time(2024);

        context.reset_population(12);

        // Let's set some ages for the population.
        // We will create a pair of male and female persons with sequential ages
        for (size_t i = 0, j = 15; i < context.population().size(); i = i + 2, j++) {
            context.population()[i].age = static_cast<unsigned>(j);
            context.population()[i + 1].age = static_cast<unsigned>(j);
        }

        // Let's set half the population gender to male, and the other half to female
        for (size_t i = 0; i < context.population().size(); i = i + 2) {
            context.population()[i].gender = core::Gender::male;
            context.population()[i + 1].gender = core::Gender::female;
        }

        // For each person, we need to set the risk factors which we can get from channels_
        for (size_t i = 0; i < context.population().size(); i++) {
            for (const auto &factor : context.mapping().entries()) {
                context.population()[i].risk_factors[factor.key()] = 1.0 + i;
            }
        }

        auto ses_module = build_ses_noise_module(repository, config);
        ses_module->initialise_population(context);

        analysis_module->initialise_population(context);
    }
};

TEST_F(TestAnalysisModule, CalculateIndex) {
    // Test that the index is calculated correctly

    size_t index_1 = analysis_module->calculate_index(test_person_1);
    size_t index_2 = analysis_module->calculate_index(test_person_2);

    ASSERT_EQ(index_1, 7 * 87);
    ASSERT_EQ(index_2, 10 * 87);
}

} // namespace hgps
