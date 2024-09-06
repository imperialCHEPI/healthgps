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

    hgps::Person test_person_1 = create_test_person(21, hgps::core::Gender::male);
    hgps::Person test_person_2 = create_test_person(22, hgps::core::Gender::male);
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

        context.reset_population(3);

        // Let's set some ages for the population
        for (size_t i = 0; i < context.population().size(); i++) {
            context.population()[i].age = 20 + static_cast<unsigned int>(i);
        }

        // Let's set the population gender to male
        for (auto &person : context.population()) {
            person.gender = core::Gender::male;
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

    // analysis_module->initialise_population(context);

    size_t index_1 = analysis_module->calculate_index(test_person_1);

    ASSERT_EQ(index_1, 29);
}

} // namespace hgps
