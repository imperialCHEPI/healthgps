#ifdef _MSC_VER
#pragma warning(disable : 26439) // This kind of function should not throw. Declare it 'noexcept'
#pragma warning(disable : 26495) // Variable is uninitialized
#pragma warning(disable : 26819) // Unannotated fallthrough between switch labels
#pragma warning(disable : 26498) // The function is constexpr, mark variable constexpr if
                                 // compile-time evaluation is desired
#pragma warning(                                                                                   \
    disable : 6285) // (<non-zero constant> || <non-zero constant>) is always a non-zero constant
#endif

#include "pch.h"
#include <gtest/gtest.h>
#include <memory>

#include "HealthGPS.Core/column.h"
#include "HealthGPS.Core/column_numeric.h"
#include "HealthGPS.Core/datatable.h"
#include "HealthGPS.Input/datamanager.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/api.h"
#include "HealthGPS/event_aggregator.h"
#include "HealthGPS/modelinput.h"
#include "HealthGPS/person.h"
#include "HealthGPS/repository.h"
#include "HealthGPS/runtime_context.h"
#include "HealthGPS/scenario.h"
#include "HealthGPS/static_linear_model.h"
#include "HealthGPS/two_step_value.h"
#include "HealthGPS/univariate_visitor.h"
using namespace hgps;

// Test event aggregator for mocking- Mahima
// this for the class EventAggregator
class TestEventAggregator final : public EventAggregator {
  public:
    TestEventAggregator() = default;
    ~TestEventAggregator() override = default;

    // Prevent copying
    TestEventAggregator(const TestEventAggregator &) = delete;
    TestEventAggregator &operator=(const TestEventAggregator &) = delete;

    // Allow moving
    TestEventAggregator(TestEventAggregator &&) noexcept = default;
    TestEventAggregator &operator=(TestEventAggregator &&) noexcept = default;

    void publish(std::unique_ptr<EventMessage> /*message*/) override {}
    void publish_async(std::unique_ptr<EventMessage> /*message*/) override {}
    [[nodiscard]] std::unique_ptr<EventSubscriber>
    subscribe(EventType /*event_id*/,
              std::function<void(std::shared_ptr<EventMessage>)> /*handler*/) override {
        return nullptr;
    }
    [[nodiscard]] bool unsubscribe(const EventSubscriber & /*subscriber*/) noexcept override {
        return true;
    }
};

TEST(TestHealthGPS_Population, CreateDefaultPerson) {
    using namespace hgps;

    auto p = Person{};
    ASSERT_GT(p.id(), 0);
    ASSERT_EQ(0u, p.age);
    ASSERT_EQ(core::Gender::unknown, p.gender);
    ASSERT_TRUE(p.is_alive());
    ASSERT_FALSE(p.has_emigrated());
    ASSERT_TRUE(p.is_active());
    ASSERT_EQ(0u, p.time_of_death());
    ASSERT_EQ(0u, p.time_of_migration());
    ASSERT_EQ(0, p.ses);
    ASSERT_EQ(0, p.risk_factors.size());
    ASSERT_EQ(0, p.diseases.size());
}

TEST(TestHealthGPS_Population, CreateUniquePerson) {
    using namespace hgps;

    auto p1 = Person{};
    auto p2 = Person{};
    const auto &p3 = p1;

    ASSERT_GT(p1.id(), 0);
    ASSERT_GT(p2.id(), p1.id());
    ASSERT_EQ(p1.id(), p3.id());
}

TEST(TestHealthGPS_Population, PersonStateIsActive) {
    using namespace hgps;

    auto p1 = Person{};
    auto p2 = Person{};
    auto p3 = Person{};
    auto p4 = p2;

    p2.die(2022);
    p3.emigrate(2022);

    ASSERT_TRUE(p1.is_active());
    ASSERT_FALSE(p2.is_active());
    ASSERT_FALSE(p3.is_active());
    ASSERT_TRUE(p4.is_active());
}

TEST(TestHealthGPS_Population, PersonStateDeath) {
    using namespace hgps;

    auto p = Person{};
    ASSERT_TRUE(p.is_alive());
    ASSERT_TRUE(p.is_active());
    ASSERT_FALSE(p.has_emigrated());
    ASSERT_EQ(0u, p.time_of_death());
    ASSERT_EQ(0u, p.time_of_migration());

    constexpr auto time_now = 2022u;
    p.die(time_now);

    ASSERT_FALSE(p.is_alive());
    ASSERT_FALSE(p.is_active());
    ASSERT_FALSE(p.has_emigrated());
    ASSERT_EQ(time_now, p.time_of_death());
    ASSERT_EQ(0u, p.time_of_migration());
    ASSERT_THROW(p.die(time_now), std::logic_error);
    ASSERT_THROW(p.emigrate(time_now), std::logic_error);
}

TEST(TestHealthGPS_Population, PersonStateEmigrated) {
    using namespace hgps;

    auto p = Person{};
    ASSERT_TRUE(p.is_alive());
    ASSERT_TRUE(p.is_active());
    ASSERT_FALSE(p.has_emigrated());
    ASSERT_EQ(0u, p.time_of_death());
    ASSERT_EQ(0u, p.time_of_migration());

    constexpr auto time_now = 2022u;
    p.emigrate(time_now);

    ASSERT_TRUE(p.is_alive());
    ASSERT_FALSE(p.is_active());
    ASSERT_TRUE(p.has_emigrated());
    ASSERT_EQ(0u, p.time_of_death());
    ASSERT_EQ(time_now, p.time_of_migration());
    ASSERT_THROW(p.die(time_now), std::logic_error);
    ASSERT_THROW(p.emigrate(time_now), std::logic_error);
}

TEST(TestHealthGPS_Population, CreateDefaultTwoStepValue) {
    using namespace hgps;

    auto v = TwoStepValue<int>{};
    ASSERT_EQ(0, v());
    ASSERT_EQ(0, v.value());
    ASSERT_EQ(0, v.old_value());
}

TEST(TestHealthGPS_Population, CreateCustomTwoStepValue) {
    using namespace hgps;

    constexpr auto initial_value = 5;
    auto v = TwoStepValue<int>{initial_value};
    ASSERT_EQ(initial_value, v());
    ASSERT_EQ(initial_value, v.value());
    ASSERT_EQ(0, v.old_value());
}

TEST(TestHealthGPS_Population, AssignToTwoStepValues) {
    using namespace hgps;

    constexpr auto initial_value = 5;
    constexpr auto next_value = 10;
    constexpr auto increment = 3;
    auto v = TwoStepValue<int>{initial_value};
    v = next_value;
    v.set_value(v.value() + increment);

    ASSERT_EQ(next_value + increment, v());
    ASSERT_EQ(next_value + increment, v.value());
    ASSERT_EQ(next_value, v.old_value());
}

TEST(TestHealthGPS_Population, SetBothTwoStepValues) {
    using namespace hgps;

    constexpr auto value = 15;
    auto v = TwoStepValue<int>{};
    v.set_both_values(value);

    ASSERT_EQ(value, v());
    ASSERT_EQ(value, v.value());
    ASSERT_EQ(value, v.old_value());
}

TEST(TestHealthGPS_Population, CloneTwoStepValues) {
    using namespace hgps;

    constexpr auto initial_value = 5;
    constexpr auto next_value = 10;
    auto source = TwoStepValue<int>{initial_value};
    source = next_value;

    auto clone = source.clone();

    ASSERT_EQ(source(), clone());
    ASSERT_EQ(source.value(), clone.value());
    ASSERT_EQ(source.old_value(), clone.old_value());
    ASSERT_NE(std::addressof(source), std::addressof(clone));
}

TEST(TestHealthGPS_Population, CreateDefaultDisease) {
    using namespace hgps;

    auto v = Disease{};

    ASSERT_EQ(DiseaseStatus::free, v.status);
    ASSERT_EQ(0, v.start_time);
}

TEST(TestHealthGPS_Population, CloneDiseaseType) {
    using namespace hgps;

    constexpr auto start_year = 2021;
    auto source = Disease{.status = DiseaseStatus::active, .start_time = start_year};
    auto clone = source.clone();

    ASSERT_EQ(source.status, clone.status);
    ASSERT_EQ(source.start_time, clone.start_time);
    ASSERT_NE(std::addressof(source), std::addressof(clone));
}

TEST(TestHealthGPS_Population, AddSingleNewEntity) {
    using namespace hgps;

    constexpr auto init_size = 10;
    constexpr auto time_now = 2022;

    auto p = Population{init_size};
    ASSERT_EQ(p.initial_size(), p.current_active_size());

    auto start_size = p.size();
    p.add(Person{core::Gender::male}, time_now);
    ASSERT_GT(p.size(), start_size);

    p[start_size].die(time_now);
    ASSERT_FALSE(p[start_size].is_active());

    auto next_time = time_now + 1;
    auto current_size = p.size();
    p.add(Person{core::Gender::female}, next_time);
    ASSERT_EQ(p.size(), current_size);
    ASSERT_TRUE(p[start_size].is_active());
}

TEST(TestHealthGPS_Population, AddMultipleNewEntities) {
    using namespace hgps;

    constexpr auto init_size = 10;
    constexpr auto alocate_size = 3;
    constexpr auto replace_size = 2;
    constexpr auto expected_size = init_size + alocate_size;

    static_assert(replace_size < alocate_size);

    auto p = Population{init_size};
    ASSERT_EQ(p.initial_size(), p.current_active_size());

    auto time_now = 2022;
    auto start_size = p.size();
    auto midpoint = start_size / 2;
    p.add_newborn_babies(alocate_size, core::Gender::male, time_now);
    ASSERT_GT(p.size(), start_size);

    p[midpoint].emigrate(time_now);
    p[start_size].die(time_now);
    ASSERT_FALSE(p[midpoint].is_active());
    ASSERT_FALSE(p[start_size].is_active());

    time_now++;
    auto current_size = p.size();
    p.add_newborn_babies(replace_size, core::Gender::female, time_now);
    ASSERT_EQ(current_size, p.size());
    ASSERT_EQ(expected_size, p.size());
    ASSERT_TRUE(p[midpoint].is_active());
    ASSERT_TRUE(p[start_size].is_active());
}

TEST(TestHealthGPS_Population, PersonIncomeValues) {
    using namespace hgps;

    auto p = Person{};
    p.income_category = core::Income::low;
    ASSERT_EQ(1.0f, p.income_to_value());

    p.income_category = core::Income::lowermiddle;
    ASSERT_EQ(2.0f, p.income_to_value());

    p.income_category = core::Income::uppermiddle;
    ASSERT_EQ(3.0f, p.income_to_value());

    p.income_category = core::Income::high;
    ASSERT_EQ(4.0f, p.income_to_value());

    p.income_category = core::Income::unknown;
    ASSERT_THROW(p.income_to_value(), core::HgpsException);
}

TEST(TestHealthGPS_Population, RegionModelOperations) {
    using namespace hgps;

    auto p = Person{};
    p.region = core::Region::England;
    ASSERT_EQ(1.0f, p.region_to_value());

    p.region = core::Region::Wales;
    ASSERT_EQ(2.0f, p.region_to_value());

    p.region = core::Region::Scotland;
    ASSERT_EQ(3.0f, p.region_to_value());

    p.region = core::Region::NorthernIreland;
    ASSERT_EQ(4.0f, p.region_to_value());

    p.region = core::Region::unknown;
    ASSERT_THROW(p.region_to_value(), core::HgpsException);
}

TEST(TestHealthGPS_Population, RegionModelParsing) {
    using namespace hgps;
    using namespace hgps::input;

    ASSERT_EQ(core::Region::England, parse_region("England"));
    ASSERT_EQ(core::Region::Wales, parse_region("Wales"));
    ASSERT_EQ(core::Region::Scotland, parse_region("Scotland"));
    ASSERT_EQ(core::Region::NorthernIreland, parse_region("NorthernIreland"));
    ASSERT_THROW(parse_region("Invalid"), core::HgpsException);
}

TEST(TestHealthGPS_Population, PersonRegionCloning) {
    using namespace hgps;

    auto source = Person{};
    source.region = core::Region::England;
    source.age = 25;
    source.gender = core::Gender::male;
    source.ses = 0.5;
    source.sector = core::Sector::urban;
    source.income_category = core::Income::high;

    auto clone = Person{};
    clone.region = source.region;
    clone.age = source.age;
    clone.gender = source.gender;
    clone.ses = source.ses;
    clone.sector = source.sector;
    clone.income_category = source.income_category;

    ASSERT_EQ(clone.region, source.region);
    ASSERT_EQ(clone.region_to_value(), source.region_to_value());
    ASSERT_EQ(clone.age, source.age);
    ASSERT_EQ(clone.gender, source.gender);
    ASSERT_EQ(clone.ses, source.ses);
    ASSERT_EQ(clone.sector, source.sector);
    ASSERT_EQ(clone.income_category, source.income_category);
}

TEST(TestHealthGPS_Population, PersonEthnicityValues) {
    using namespace hgps;

    auto p = Person{};
    p.ethnicity = core::Ethnicity::White;
    ASSERT_EQ(1.0f, p.ethnicity_to_value());

    p.ethnicity = core::Ethnicity::Asian;
    ASSERT_EQ(2.0f, p.ethnicity_to_value());

    p.ethnicity = core::Ethnicity::Black;
    ASSERT_EQ(3.0f, p.ethnicity_to_value());

    p.ethnicity = core::Ethnicity::Others;
    ASSERT_EQ(4.0f, p.ethnicity_to_value());

    p.ethnicity = core::Ethnicity::unknown;
    ASSERT_THROW(p.ethnicity_to_value(), core::HgpsException);
}

TEST(TestHealthGPS_Population, EthnicityModelParsing) {
    using namespace hgps;
    using namespace hgps::input;

    ASSERT_EQ(core::Ethnicity::White, parse_ethnicity("White"));
    ASSERT_EQ(core::Ethnicity::Asian, parse_ethnicity("Asian"));
    ASSERT_EQ(core::Ethnicity::Black, parse_ethnicity("Black"));
    ASSERT_EQ(core::Ethnicity::Others, parse_ethnicity("Others"));
    ASSERT_THROW(parse_ethnicity("Invalid"), core::HgpsException);
}

class TestScenario final : public Scenario {
  public:
    TestScenario() = default;
    ~TestScenario() override = default;

    // Prevent copying
    TestScenario(const TestScenario &) = delete;
    TestScenario &operator=(const TestScenario &) = delete;

    // Allow moving
    TestScenario(TestScenario &&) noexcept = default;
    TestScenario &operator=(TestScenario &&) noexcept = default;

    [[nodiscard]] ScenarioType type() noexcept override { return ScenarioType::baseline; }
    [[nodiscard]] std::string name() override { return "Test"; }
    [[nodiscard]] SyncChannel &channel() override { return channel_; }
    void clear() noexcept override {}
    [[nodiscard]] double apply([[maybe_unused]] Random &generator, [[maybe_unused]] Person &entity,
                               [[maybe_unused]] int time,
                               [[maybe_unused]] const core::Identifier &risk_factor_key,
                               double value) override {
        return value;
    }

  private:
    SyncChannel channel_{};
};

std::shared_ptr<ModelInput> create_test_modelinput() {
    // Create data table with required columns
    core::DataTable data;

    // Add region column and probabilities
    std::vector<std::string> region_data{"England", "Wales", "Scotland", "NorthernIreland"};
    auto region_col = std::make_unique<hgps::core::PrimitiveDataTableColumn<std::string>>(
        "region", std::move(region_data));
    data.add(std::move(region_col));

    std::vector<double> region_prob_data{0.5, 0.2, 0.2, 0.1};
    auto region_prob_col = std::make_unique<hgps::core::PrimitiveDataTableColumn<double>>(
        "region_prob", std::move(region_prob_data));
    data.add(std::move(region_prob_col));

    // Add ethnicity column and probabilities
    std::vector<std::string> ethnicity_data{"White", "Asian", "Black", "Others"};
    auto ethnicity_col = std::make_unique<hgps::core::PrimitiveDataTableColumn<std::string>>(
        "ethnicity", std::move(ethnicity_data));
    data.add(std::move(ethnicity_col));

    std::vector<double> ethnicity_prob_data{0.5, 0.25, 0.15, 0.1};
    auto ethnicity_prob_col = std::make_unique<hgps::core::PrimitiveDataTableColumn<double>>(
        "ethnicity_prob", std::move(ethnicity_prob_data));
    data.add(std::move(ethnicity_prob_col));

    // Create JSON configuration for demographic coefficients
    nlohmann::json config = {
        {"modelling",
         {{"demographic_models",
           {{"region",
             {{"probabilities",
               {{"coefficients", {{"age", 0.0}, {"gender", {{"male", 0.0}, {"female", 0.0}}}}}}}}},
            {"ethnicity",
             {{"probabilities",
               {{"coefficients",
                 {{"age", 0.0},
                  {"gender", {{"male", 0.0}, {"female", 0.0}}},
                  {"region",
                   {{"England", 0.0}, {"Wales", 0.0}, {"Scotland", 0.0}, {"NorthernIreland", 0.0}}},
                  {"ethnicity",
                   {{"White", 0.0}, {"Asian", 0.0}, {"Black", 0.0}, {"Others", 0.0}}}}}}}}}}}}}};

    // Load demographic coefficients
    data.load_demographic_coefficients(config);

    // Create model input with the initialized data table
    core::IntegerInterval age_range{0, 100};
    Settings settings{static_cast<core::Country>(0), 1.0f, age_range};
    RunInfo run_info;
    SESDefinition ses_info{"linear", {1.0}};
    std::vector<MappingEntry> entries;
    entries.emplace_back("test", 0, std::nullopt);
    HierarchicalMapping risk_mapping{std::move(entries)};
    std::vector<core::DiseaseInfo> diseases;

    return std::make_shared<ModelInput>(std::move(data), std::move(settings), run_info,
                                        std::move(ses_info), std::move(risk_mapping),
                                        std::move(diseases));
}

TEST(TestHealthGPS_Population, RegionProbabilities) {
    using namespace hgps;

    // Create mock objects needed for RuntimeContext
    auto bus = std::make_shared<TestEventAggregator>();
    auto inputs = create_test_modelinput();
    auto scenario = std::make_unique<TestScenario>();

    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    auto probs = context.get_region_probabilities(30, core::Gender::male);

    // Check probabilities sum to 1.0
    double sum = 0.0;
    for (const auto &[region, prob] : probs) {
        sum += prob;
    }
    ASSERT_NEAR(1.0, sum, 0.0001);

    // Check all regions have valid probabilities
    for (const auto &[region, prob] : probs) {
        ASSERT_GE(prob, 0.0);
        ASSERT_LE(prob, 1.0);
    }
}

TEST(TestHealthGPS_Population, EthnicityProbabilities) {
    using namespace hgps;

    // Create mock objects needed for RuntimeContext
    auto bus = std::make_shared<TestEventAggregator>();
    auto inputs = create_test_modelinput();
    auto scenario = std::make_unique<TestScenario>();

    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    auto probs = context.get_ethnicity_probabilities(30, core::Gender::male, core::Region::England);

    // Check probabilities sum to 1.0
    double sum = 0.0;
    for (const auto &[ethnicity, prob] : probs) {
        sum += prob;
    }
    ASSERT_NEAR(1.0, sum, 0.0001);

    // Check all ethnicities have valid probabilities
    for (const auto &[ethnicity, prob] : probs) {
        ASSERT_GE(prob, 0.0);
        ASSERT_LE(prob, 1.0);
    }
}

// Tests for person.cpp - Mahima
// Verifies basic person operations including risk factor management
TEST(TestPerson, RiskFactors) {
    using namespace hgps;

    Person person;

    // Set risk factors using the risk_factors map
    person.risk_factors["BMI"_id] = 25.0;
    person.risk_factors["PhysicalActivity"_id] = 150.0;

    // Test risk factor access
    ASSERT_EQ(25.0, person.get_risk_factor_value("BMI"_id));
    ASSERT_EQ(150.0, person.get_risk_factor_value("PhysicalActivity"_id));

    // Test invalid risk factor
    ASSERT_THROW(person.get_risk_factor_value("Unknown"_id), std::out_of_range);
}

// Tests for person.cpp - Mahima
// Tests copy operations between Person objects
TEST(TestPerson, CopyOperations) {
    using namespace hgps;

    Person source;
    source.age = 25;
    source.gender = core::Gender::female;
    source.region = core::Region::Scotland;
    source.ethnicity = core::Ethnicity::Asian;
    source.risk_factors["BMI"_id] = 22.0;
    source.risk_factors["PhysicalActivity"_id] = 200.0;

    Person destination;
    destination.copy_from(source);

    ASSERT_EQ(source.age, destination.age);
    ASSERT_EQ(source.gender, destination.gender);
    ASSERT_EQ(source.region, destination.region);
    ASSERT_EQ(source.ethnicity, destination.ethnicity);
    ASSERT_EQ(source.risk_factors["BMI"_id], destination.risk_factors["BMI"_id]);
    ASSERT_EQ(source.risk_factors["PhysicalActivity"_id],
              destination.risk_factors["PhysicalActivity"_id]);
}

// Tests for person.cpp - Mahima
// Tests basic person operations and constructors
TEST(TestPerson, BasicOperations) {
    using namespace hgps;

    // Test default constructor
    Person person;
    ASSERT_TRUE(person.is_alive());
    ASSERT_FALSE(person.has_emigrated());
    ASSERT_TRUE(person.is_active());
    ASSERT_EQ(0u, person.time_of_death());
    ASSERT_EQ(0u, person.time_of_migration());

    // Test gender-specific constructor
    Person male_person(core::Gender::male);
    ASSERT_EQ(1.0f, male_person.gender_to_value());
    ASSERT_EQ("male", male_person.gender_to_string());

    Person female_person(core::Gender::female);
    ASSERT_EQ(0.0f, female_person.gender_to_value());
    ASSERT_EQ("female", female_person.gender_to_string());
}

// Tests for person.cpp - Mahima
// Tests age-based operations
TEST(TestPerson, AgeBasedOperations) {
    using namespace hgps;

    Person person;
    person.age = 15;
    ASSERT_FALSE(person.over_18());

    person.age = 18;
    ASSERT_TRUE(person.over_18());

    person.age = 25;
    ASSERT_TRUE(person.over_18());
}

// Tests for person.cpp - Mahima
// Tests region-related operations
TEST(TestPerson, RegionOperations) {
    using namespace hgps;

    Person person;

    person.region = core::Region::England;
    ASSERT_EQ(1.0f, person.region_to_value());

    person.region = core::Region::Wales;
    ASSERT_EQ(2.0f, person.region_to_value());

    person.region = core::Region::Scotland;
    ASSERT_EQ(3.0f, person.region_to_value());

    person.region = core::Region::NorthernIreland;
    ASSERT_EQ(4.0f, person.region_to_value());
}

// Tests for person.cpp - Mahima
// Tests ethnicity-related operations
TEST(TestPerson, EthnicityOperations) {
    using namespace hgps;

    Person person;

    person.ethnicity = core::Ethnicity::White;
    ASSERT_EQ(1.0f, person.ethnicity_to_value());

    person.ethnicity = core::Ethnicity::Black;
    ASSERT_EQ(3.0f, person.ethnicity_to_value());

    person.ethnicity = core::Ethnicity::Asian;
    ASSERT_EQ(2.0f, person.ethnicity_to_value());

    person.ethnicity = core::Ethnicity::Others;
    ASSERT_EQ(4.0f, person.ethnicity_to_value());
}

// Tests for person.cpp - Mahima
// Tests life events like death and emigration
TEST(TestPerson, LifeEvents) {
    using namespace hgps;

    Person person;
    ASSERT_TRUE(person.is_active());

    // Test emigration
    person.emigrate(10);
    ASSERT_TRUE(person.has_emigrated());
    ASSERT_FALSE(person.is_active());
    ASSERT_EQ(10u, person.time_of_migration());

    // Test death
    Person another_person;
    another_person.die(20);
    ASSERT_FALSE(another_person.is_alive());
    ASSERT_FALSE(another_person.is_active());
    ASSERT_EQ(20u, another_person.time_of_death());
}

// Tests for runtime_context.cpp - Mahima
// Tests initialization and configuration of runtime context
TEST(TestRuntimeContext, BasicOperations) {
    using namespace hgps;

    auto bus = std::make_shared<TestEventAggregator>();
    auto inputs = create_test_modelinput();
    auto scenario = std::make_unique<TestScenario>();

    auto context = RuntimeContext(bus, inputs, std::move(scenario));
    ASSERT_EQ(0, context.time_now());
    ASSERT_EQ(ScenarioType::baseline, context.scenario().type());
}

// Tests for runtime_context.cpp - Mahima
// Tests demographic model operations
TEST(TestRuntimeContext, DemographicModels) {
    using namespace hgps;

    auto bus = std::make_shared<TestEventAggregator>();
    auto inputs = create_test_modelinput();
    auto scenario = std::make_unique<TestScenario>();

    auto context = RuntimeContext(bus, inputs, std::move(scenario));

    // Test region probabilities
    auto region_probs = context.get_region_probabilities(25, core::Gender::male);
    ASSERT_FALSE(region_probs.empty());

    // Test ethnicity probabilities
    auto ethnicity_probs =
        context.get_ethnicity_probabilities(25, core::Gender::male, core::Region::England);
    ASSERT_FALSE(ethnicity_probs.empty());
}

// Tests for simulation.cpp - Mahima
// Tests basic simulation setup and configuration
TEST(TestSimulation, BasicSetup) {
    using namespace hgps;

    auto bus = std::make_shared<TestEventAggregator>();
    auto inputs = create_test_modelinput();
    auto scenario = std::make_unique<TestScenario>();

    auto manager = std::make_shared<input::DataManager>("test_data");
    auto repository = std::make_shared<CachedRepository>(*manager);
    auto factory = get_default_simulation_module_factory(*repository);

    auto simulation = Simulation(factory, bus, inputs, std::move(scenario));

    // Initialize simulation through adevs::Simulator
    auto env = adevs::Simulator<int>();
    simulation.init(&env);

    ASSERT_EQ(0, env.now().real);
    ASSERT_EQ(ScenarioType::baseline, simulation.type());
}

// Tests for simulation.cpp - Mahima
// Tests population initialization in simulation
TEST(TestSimulation, PopulationInitialization) {
    using namespace hgps;

    auto bus = std::make_shared<TestEventAggregator>();
    auto inputs = create_test_modelinput();
    auto scenario = std::make_unique<TestScenario>();

    auto manager = std::make_shared<input::DataManager>("test_data");
    auto repository = std::make_shared<CachedRepository>(*manager);
    auto factory = get_default_simulation_module_factory(*repository);

    auto simulation = Simulation(factory, bus, inputs, std::move(scenario));

    // Initialize simulation through adevs::Simulator
    auto env = adevs::Simulator<int>();
    simulation.init(&env);

    ASSERT_EQ(0, env.now().real);
    ASSERT_EQ(ScenarioType::baseline, simulation.type());
}

// Tests for static_linear_model.cpp - Mahima
// Tests basic operations of static linear model
TEST(TestStaticLinearModel, BasicOperations) {
    using namespace hgps;

    // Create a minimal model definition
    std::vector<core::Identifier> names{"test"};
    std::vector<LinearModelParams> models{LinearModelParams{}};
    std::vector<core::DoubleInterval> ranges{core::DoubleInterval(0.0, 1.0)};
    std::vector<double> lambda{1.0};
    std::vector<double> stddev{1.0};
    Eigen::MatrixXd cholesky(1, 1);
    cholesky(0, 0) = 1.0;

    auto definition = StaticLinearModelDefinition(
        std::make_unique<RiskFactorSexAgeTable>(),
        std::make_unique<std::unordered_map<core::Identifier, double>>(),
        std::make_unique<std::unordered_map<core::Identifier, int>>(),
        std::make_shared<std::unordered_map<core::Identifier, double>>(), std::move(names),
        std::move(models), std::move(ranges), std::move(lambda), std::move(stddev),
        std::move(cholesky), std::vector<LinearModelParams>{}, std::vector<core::DoubleInterval>{},
        Eigen::MatrixXd(0, 0), std::make_unique<std::vector<LinearModelParams>>(),
        std::make_unique<std::vector<core::DoubleInterval>>(),
        std::make_unique<std::vector<double>>(), 1.0,
        std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>{},
        std::unordered_map<core::Income, LinearModelParams>{},
        std::unordered_map<core::Region, LinearModelParams>{}, 1.0, 1.0,
        std::unordered_map<core::Ethnicity, LinearModelParams>{});

    auto model = definition.create_model();
    ASSERT_TRUE(model != nullptr);
    ASSERT_EQ(RiskFactorModelType::Static, model->type());
    ASSERT_EQ("Static", model->name());
}

// Tests for static_linear_model.cpp - Mahima
// Tests error handling in static linear model
TEST(TestStaticLinearModel, ErrorHandling) {
    using namespace hgps;

    // Test with empty names vector
    ASSERT_THROW(
        StaticLinearModelDefinition(
            std::make_unique<RiskFactorSexAgeTable>(),
            std::make_unique<std::unordered_map<core::Identifier, double>>(),
            std::make_unique<std::unordered_map<core::Identifier, int>>(),
            std::make_shared<std::unordered_map<core::Identifier, double>>(),
            std::vector<core::Identifier>{}, std::vector<LinearModelParams>{},
            std::vector<core::DoubleInterval>{}, std::vector<double>{}, std::vector<double>{},
            Eigen::MatrixXd(0, 0), std::vector<LinearModelParams>{},
            std::vector<core::DoubleInterval>{}, Eigen::MatrixXd(0, 0),
            std::make_unique<std::vector<LinearModelParams>>(),
            std::make_unique<std::vector<core::DoubleInterval>>(),
            std::make_unique<std::vector<double>>(), 1.0,
            std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>{},
            std::unordered_map<core::Income, LinearModelParams>{},
            std::unordered_map<core::Region, LinearModelParams>{}, 1.0, 1.0,
            std::unordered_map<core::Ethnicity, LinearModelParams>{}),
        core::HgpsException);
}

// Tests for static_linear_model.cpp - Mahima
// Tests model validation and boundary conditions
TEST(TestStaticLinearModel, Validation) {
    using namespace hgps;

    // Create a minimal valid model definition
    std::vector<core::Identifier> names{"test"};
    std::vector<LinearModelParams> models{LinearModelParams{}};
    std::vector<core::DoubleInterval> ranges{core::DoubleInterval(0.0, 1.0)};
    std::vector<double> lambda{1.0};
    std::vector<double> stddev{1.0};
    Eigen::MatrixXd cholesky(1, 1);
    cholesky(0, 0) = 1.0;

    auto definition = StaticLinearModelDefinition(
        std::make_unique<RiskFactorSexAgeTable>(),
        std::make_unique<std::unordered_map<core::Identifier, double>>(),
        std::make_unique<std::unordered_map<core::Identifier, int>>(),
        std::make_shared<std::unordered_map<core::Identifier, double>>(), std::move(names),
        std::move(models), std::move(ranges), std::move(lambda), std::move(stddev),
        std::move(cholesky), std::vector<LinearModelParams>{}, std::vector<core::DoubleInterval>{},
        Eigen::MatrixXd(0, 0), std::make_unique<std::vector<LinearModelParams>>(),
        std::make_unique<std::vector<core::DoubleInterval>>(),
        std::make_unique<std::vector<double>>(), 1.0,
        std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>{},
        std::unordered_map<core::Income, LinearModelParams>{},
        std::unordered_map<core::Region, LinearModelParams>{}, 1.0, 1.0,
        std::unordered_map<core::Ethnicity, LinearModelParams>{});

    auto model = definition.create_model();
    ASSERT_TRUE(model != nullptr);
}
// Tests for univariate_visitor.cpp - Mahima
// Tests univariate visitor operations
TEST(UnivariateVisitor, StringColumnVisit) {
    // Create a string column using builder
    hgps::core::StringDataTableColumnBuilder builder("test_string");
    builder.append("value1");
    builder.append("value2");
    auto column = builder.build();

    // Create visitor
    hgps::UnivariateVisitor visitor;

    // String columns should throw exception
    ASSERT_THROW(visitor.visit(*column), std::invalid_argument);
}

TEST(UnivariateVisitor, FloatColumnVisit) {
    // Create a float column using builder
    hgps::core::FloatDataTableColumnBuilder builder("test_float");
    builder.append(1.5f);
    builder.append(2.5f);
    builder.append(3.5f);
    auto column = builder.build();

    // Create visitor
    hgps::UnivariateVisitor visitor;

    // Visit should succeed and calculate summary
    ASSERT_NO_THROW(visitor.visit(*column));

    auto summary = visitor.get_summary();
    ASSERT_EQ(summary.name(), "test_float");
    ASSERT_EQ(summary.count_valid(), 3);
    ASSERT_NEAR(summary.min(), 1.5f, 0.0001f);
    ASSERT_NEAR(summary.max(), 3.5f, 0.0001f);
    ASSERT_NEAR(summary.average(), 2.5f, 0.0001f);
}

TEST(UnivariateVisitor, DoubleColumnVisit) {
    // Create a double column using builder
    hgps::core::DoubleDataTableColumnBuilder builder("test_double");
    builder.append(1.5);
    builder.append(2.5);
    builder.append(3.5);
    builder.append(4.5);
    auto column = builder.build();

    // Create visitor
    hgps::UnivariateVisitor visitor;

    // Visit should succeed and calculate summary
    ASSERT_NO_THROW(visitor.visit(*column));

    auto summary = visitor.get_summary();
    ASSERT_EQ(summary.name(), "test_double");
    ASSERT_EQ(summary.count_valid(), 4);
    ASSERT_NEAR(summary.min(), 1.5, 0.0001);
    ASSERT_NEAR(summary.max(), 4.5, 0.0001);
    ASSERT_NEAR(summary.average(), 3.0, 0.0001);
}

// Tests for column_numeric.h - Mahima
// Tests constructors and type names for specialized column types
TEST(TestColumnNumeric, ConstructorsAndTypes) {
    using namespace hgps::core;

    // Test string column constructor and type
    std::vector<std::string> str_data{"test1", "test2"};
    auto str_col = std::make_unique<StringDataTableColumn>("string_col", std::move(str_data));
    ASSERT_EQ("string", str_col->type());
    ASSERT_EQ("string_col", str_col->name());
    ASSERT_EQ(2u, str_col->size());

    // Test float column constructor and type
    std::vector<float> float_data{1.5f, 2.5f};
    auto float_col = std::make_unique<FloatDataTableColumn>("float_col", std::move(float_data));
    ASSERT_EQ("float", float_col->type());
    ASSERT_EQ("float_col", float_col->name());
    ASSERT_EQ(2u, float_col->size());

    // Test double column constructor and type
    std::vector<double> double_data{1.5, 2.5};
    auto double_col = std::make_unique<DoubleDataTableColumn>("double_col", std::move(double_data));
    ASSERT_EQ("double", double_col->type());
    ASSERT_EQ("double_col", double_col->name());
    ASSERT_EQ(2u, double_col->size());

    // Test integer column constructor and type
    std::vector<int> int_data{1, 2};
    auto int_col = std::make_unique<IntegerDataTableColumn>("int_col", std::move(int_data));
    ASSERT_EQ("integer", int_col->type());
    ASSERT_EQ("int_col", int_col->name());
    ASSERT_EQ(2u, int_col->size());
}

// Test column validation and error handling
TEST(TestColumnNumeric, ValidationAndErrors) {
    using namespace hgps::core;

    // Test invalid column name (too short)
    std::vector<std::string> data{"test"};
    ASSERT_THROW(StringDataTableColumn("a", std::move(data)), std::invalid_argument);

    // Test invalid column name (starts with number)
    std::vector<std::string> data2{"test"};
    ASSERT_THROW(StringDataTableColumn("1name", std::move(data2)), std::invalid_argument);

    // Test with null bitmap size mismatch
    std::vector<double> data3{1.0, 2.0};
    std::vector<bool> bitmap{true}; // Wrong size
    ASSERT_THROW(DoubleDataTableColumn("test_col", std::move(data3), std::move(bitmap)),
                 std::out_of_range);
}

// Test column data access and null handling
TEST(TestColumnNumeric, DataAccessAndNulls) {
    using namespace hgps::core;

    std::vector<double> data{1.0, 2.0, 3.0};
    std::vector<bool> bitmap{true, false, true};
    auto col =
        std::make_unique<DoubleDataTableColumn>("test_col", std::move(data), std::move(bitmap));

    // Test valid data access
    ASSERT_TRUE(col->is_valid(0));
    ASSERT_EQ(1.0, std::any_cast<double>(col->value(0)));

    // Test null value
    ASSERT_FALSE(col->is_valid(1));
    ASSERT_TRUE(col->is_null(1));

    // Test out of bounds
    ASSERT_TRUE(col->is_null(10));
    ASSERT_FALSE(col->is_valid(10));
}

// Test column cloning functionality
TEST(TestColumnNumeric, Cloning) {
    using namespace hgps::core;

    // Create original column with data and null bitmap
    std::vector<double> data{1.0, 2.0, 3.0};
    std::vector<bool> bitmap{true, false, true};
    auto original =
        std::make_unique<DoubleDataTableColumn>("test_col", std::move(data), std::move(bitmap));

    // Clone the column
    auto clone = original->clone();

    // Verify clone has same properties
    ASSERT_EQ(original->name(), clone->name());
    ASSERT_EQ(original->type(), clone->type());
    ASSERT_EQ(original->size(), clone->size());
    ASSERT_EQ(original->null_count(), clone->null_count());

    // Verify data values match
    for (size_t i = 0; i < original->size(); ++i) {
        ASSERT_EQ(original->is_valid(i), clone->is_valid(i));
        if (original->is_valid(i)) {
            ASSERT_EQ(std::any_cast<double>(original->value(i)),
                      std::any_cast<double>(clone->value(i)));
        }
    }
}

// Enhanced tests for column_primitive.h visitor coverage - Mahima
TEST(TestColumnPrimitive, ComprehensiveVisitorImplementation) {
    using namespace hgps::core;

    // Create test columns with null values to ensure full coverage
    std::vector<std::string> str_data{"test1", "test2"};
    std::vector<bool> str_bitmap{true, false};
    auto str_col = std::make_unique<StringDataTableColumn>("string_col", std::move(str_data),
                                                           std::move(str_bitmap));

    std::vector<float> float_data{1.5f, 2.5f};
    std::vector<bool> float_bitmap{false, true};
    auto float_col = std::make_unique<FloatDataTableColumn>("float_col", std::move(float_data),
                                                            std::move(float_bitmap));

    std::vector<double> double_data{1.5, 2.5};
    std::vector<bool> double_bitmap{true, true};
    auto double_col = std::make_unique<DoubleDataTableColumn>("double_col", std::move(double_data),
                                                              std::move(double_bitmap));

    std::vector<int> int_data{1, 2};
    std::vector<bool> int_bitmap{true, false};
    auto int_col = std::make_unique<IntegerDataTableColumn>("int_col", std::move(int_data),
                                                            std::move(int_bitmap));

    // Create a visitor that verifies both valid and null values
    class ComprehensiveVisitor : public DataTableColumnVisitor {
      public:
        void visit(const StringDataTableColumn &col) override {
            ASSERT_TRUE(col.is_valid(0));
            ASSERT_FALSE(col.is_valid(1));
            ASSERT_EQ("test1", std::any_cast<std::string>(col.value(0)));
        }

        void visit(const FloatDataTableColumn &col) override {
            ASSERT_FALSE(col.is_valid(0));
            ASSERT_TRUE(col.is_valid(1));
            ASSERT_EQ(2.5f, std::any_cast<float>(col.value(1)));
        }

        void visit(const DoubleDataTableColumn &col) override {
            ASSERT_TRUE(col.is_valid(0));
            ASSERT_TRUE(col.is_valid(1));
            ASSERT_EQ(1.5, std::any_cast<double>(col.value(0)));
            ASSERT_EQ(2.5, std::any_cast<double>(col.value(1)));
        }

        void visit(const IntegerDataTableColumn &col) override {
            ASSERT_TRUE(col.is_valid(0));
            ASSERT_FALSE(col.is_valid(1));
            ASSERT_EQ(1, std::any_cast<int>(col.value(0)));
        }
    };

    ComprehensiveVisitor visitor;

    // Test each column type's accept method with comprehensive validation
    str_col->accept(visitor);
    float_col->accept(visitor);
    double_col->accept(visitor);
    int_col->accept(visitor);
}

// Test for compile-time type constraints (line 136)
TEST(TestColumnPrimitive, TypeConstraints) {
    using namespace hgps::core;

    // Verify supported types work
    {
        std::vector<std::string> data{"test"};
        ASSERT_NO_THROW(StringDataTableColumn("test_str", std::move(data)));
    }

    {
        std::vector<float> data{1.0f};
        ASSERT_NO_THROW(FloatDataTableColumn("test_float", std::move(data)));
    }

    {
        std::vector<double> data{1.0};
        ASSERT_NO_THROW(DoubleDataTableColumn("test_double", std::move(data)));
    }

    {
        std::vector<int> data{1};
        ASSERT_NO_THROW(IntegerDataTableColumn("test_int", std::move(data)));
    }

    // Note: Unsupported types are checked at compile-time
    // The following would fail to compile:
    // PrimitiveDataTableColumn<unsigned int>
    // PrimitiveDataTableColumn<char>
    // PrimitiveDataTableColumn<bool>
}

// Tests for datatable.cpp parsing functions - Mahima
// Tests internal parsing functions for region and ethnicity
TEST(TestDataTable, RegionParsing) {
    using namespace hgps;
    using namespace hgps::input;

    // Test valid region parsing
    ASSERT_EQ(core::Region::England, parse_region("England"));
    ASSERT_EQ(core::Region::Wales, parse_region("Wales"));
    ASSERT_EQ(core::Region::Scotland, parse_region("Scotland"));
    ASSERT_EQ(core::Region::NorthernIreland, parse_region("NorthernIreland"));
    ASSERT_EQ(core::Region::unknown, parse_region("unknown"));

    // Test invalid region
    ASSERT_THROW(parse_region("InvalidRegion"), core::HgpsException);
    ASSERT_THROW(parse_region(""), core::HgpsException);
}

TEST(TestDataTable, EthnicityParsing) {
    using namespace hgps;
    using namespace hgps::input;

    // Test valid ethnicity parsing
    ASSERT_EQ(core::Ethnicity::White, parse_ethnicity("White"));
    ASSERT_EQ(core::Ethnicity::Asian, parse_ethnicity("Asian"));
    ASSERT_EQ(core::Ethnicity::Black, parse_ethnicity("Black"));
    ASSERT_EQ(core::Ethnicity::Others, parse_ethnicity("Others"));
    ASSERT_EQ(core::Ethnicity::unknown, parse_ethnicity("unknown"));

    // Test invalid ethnicity
    ASSERT_THROW(parse_ethnicity("InvalidEthnicity"), core::HgpsException);
    ASSERT_THROW(parse_ethnicity(""), core::HgpsException);
}

// Tests for DataTable column operations
TEST(TestDataTable, ColumnOperations) {
    using namespace hgps::core;

    DataTable table;

    // Test adding columns
    std::vector<std::string> str_data{"test1", "test2"};
    auto str_col = std::make_unique<StringDataTableColumn>("string_col", std::move(str_data));
    ASSERT_NO_THROW(table.add(std::move(str_col)));

    // Test column size mismatch
    std::vector<double> double_data{1.0, 2.0, 3.0}; // Different size
    auto double_col = std::make_unique<DoubleDataTableColumn>("double_col", std::move(double_data));
    ASSERT_THROW(table.add(std::move(double_col)), std::invalid_argument);

    // Test duplicate column name
    std::vector<std::string> str_data2{"test3", "test4"};
    auto str_col2 = std::make_unique<StringDataTableColumn>("string_col", std::move(str_data2));
    ASSERT_THROW(table.add(std::move(str_col2)), std::invalid_argument);

    // Test column access
    ASSERT_EQ(1u, table.num_columns());
    ASSERT_EQ("string_col", table.column(0).name());
    ASSERT_THROW(table.column(1), std::out_of_range);
    ASSERT_EQ("string_col", table.column("string_col").name());
    ASSERT_THROW(table.column("nonexistent"), std::out_of_range);

    // Test column existence check
    ASSERT_TRUE(table.column_if_exists("string_col").has_value());
    ASSERT_FALSE(table.column_if_exists("nonexistent").has_value());
}

// Tests for DataTable demographic operations
TEST(TestDataTable, DemographicOperations) {
    using namespace hgps::core;

    DataTable table;

    // Add required columns for demographic operations
    std::vector<std::string> region_data{"England", "Wales"};
    auto region_col = std::make_unique<StringDataTableColumn>("region", std::move(region_data));
    table.add(std::move(region_col));

    std::vector<double> region_prob_data{0.7, 0.3};
    auto region_prob_col =
        std::make_unique<DoubleDataTableColumn>("region_prob", std::move(region_prob_data));
    table.add(std::move(region_prob_col));

    // Test demographic coefficient loading
    nlohmann::json config = {
        {"modelling",
         {{"demographic_models",
           {{"region",
             {{"probabilities",
               {{"coefficients",
                 {{"age", 0.1}, {"gender", {{"male", 0.2}, {"female", 0.3}}}}}}}}}}}}}};

    ASSERT_NO_THROW(table.load_demographic_coefficients(config));

    // Test distribution calculations
    auto region_dist = table.get_region_distribution(25, Gender::male);
    ASSERT_FALSE(region_dist.empty());

    double total_prob = 0.0;
    for (const auto &[region, prob] : region_dist) {
        ASSERT_GE(prob, 0.0);
        ASSERT_LE(prob, 1.0);
        total_prob += prob;
    }
    ASSERT_NEAR(1.0, total_prob, 0.0001);
}

// Tests for column_numeric.h constructor coverage - Mahima
TEST(TestColumnNumeric, SpecificConstructors) {
    using namespace hgps::core;

    // Test StringDataTableColumn constructor (line 15)
    {
        std::vector<std::string> data{"test1", "test2"};
        std::vector<bool> bitmap{true, false};
        auto col = StringDataTableColumn("test_str", std::move(data), std::move(bitmap));
        ASSERT_EQ(2u, col.size());
        ASSERT_EQ(1u, col.null_count());
    }

    // Test FloatDataTableColumn constructor (line 23)
    {
        std::vector<float> data{1.0f, 2.0f};
        std::vector<bool> bitmap{true, true};
        auto col = FloatDataTableColumn("test_float", std::move(data), std::move(bitmap));
        ASSERT_EQ(2u, col.size());
        ASSERT_EQ(0u, col.null_count());
    }

    // Test DoubleDataTableColumn constructor (line 31)
    {
        std::vector<double> data{1.0, 2.0};
        std::vector<bool> bitmap{false, true};
        auto col = DoubleDataTableColumn("test_double", std::move(data), std::move(bitmap));
        ASSERT_EQ(2u, col.size());
        ASSERT_EQ(1u, col.null_count());
    }

    // Test IntegerDataTableColumn constructor (line 39)
    {
        std::vector<int> data{1, 2};
        std::vector<bool> bitmap{true, false};
        auto col = IntegerDataTableColumn("test_int", std::move(data), std::move(bitmap));
        ASSERT_EQ(2u, col.size());
        ASSERT_EQ(1u, col.null_count());
    }
}

// Enhanced tests for datatable.cpp coverage - Mahima
TEST(TestDataTable, ComprehensiveOperations) {
    using namespace hgps::core;

    DataTable table;

    // Test column operations (lines 129, 148-149, 153)
    std::vector<std::string> str_data{"test1", "test2"};
    auto str_col = std::make_unique<StringDataTableColumn>("string_col", std::move(str_data));
    table.add(std::move(str_col));

    // Test column access and validation (lines 178, 186-187, 190)
    ASSERT_EQ(1u, table.num_columns());
    ASSERT_EQ("string_col", table.column(0).name());
    ASSERT_THROW(table.column(1), std::out_of_range);

    auto col_opt = table.column_if_exists("string_col");
    ASSERT_TRUE(col_opt.has_value());
    ASSERT_EQ("string_col", col_opt.value().get().name());

    // Test demographic operations (lines 197, 216-217, 221, 247, 255-256)
    std::vector<std::string> region_data{"England", "Wales", "Scotland", "NorthernIreland"};
    auto region_col = std::make_unique<StringDataTableColumn>("region", std::move(region_data));

    std::vector<double> prob_data{0.4, 0.3, 0.2, 0.1};
    auto prob_col = std::make_unique<DoubleDataTableColumn>("region_prob", std::move(prob_data));

    DataTable demo_table;
    demo_table.add(std::move(region_col));
    demo_table.add(std::move(prob_col));

    // Test demographic coefficient loading
    nlohmann::json config = {
        {"modelling",
         {{"demographic_models",
           {{"region",
             {{"probabilities",
               {{"coefficients",
                 {{"age", 0.1},
                  {"gender", {{"male", 0.2}, {"female", 0.3}}},
                  {"region",
                   {{"England", 0.4},
                    {"Wales", 0.3},
                    {"Scotland", 0.2},
                    {"NorthernIreland", 0.1}}}}}}}}},
            {"ethnicity",
             {{"probabilities",
               {{"coefficients",
                 {{"age", 0.1},
                  {"gender", {{"male", 0.2}, {"female", 0.3}}},
                  {"region",
                   {{"England", 0.4}, {"Wales", 0.3}, {"Scotland", 0.2}, {"NorthernIreland", 0.1}}},
                  {"ethnicity",
                   {{"White", 0.7}, {"Asian", 0.2}, {"Black", 0.05}, {"Others", 0.05}}}}}}}}}}}}}};

    demo_table.load_demographic_coefficients(config);

    // Test distribution calculations
    auto region_dist = demo_table.get_region_distribution(25, Gender::male);
    ASSERT_EQ(4u, region_dist.size());

    double total_prob = 0.0;
    for (const auto &[region, prob] : region_dist) {
        ASSERT_GE(prob, 0.0);
        ASSERT_LE(prob, 1.0);
        total_prob += prob;
    }
    ASSERT_NEAR(1.0, total_prob, 0.0001);

    // Test ethnicity distribution
    auto ethnicity_dist =
        demo_table.get_ethnicity_distribution(25, Gender::female, Region::England);
    ASSERT_FALSE(ethnicity_dist.empty());

    total_prob = 0.0;
    for (const auto &[ethnicity, prob] : ethnicity_dist) {
        ASSERT_GE(prob, 0.0);
        ASSERT_LE(prob, 1.0);
        total_prob += prob;
    }
    ASSERT_NEAR(1.0, total_prob, 0.0001);
}
