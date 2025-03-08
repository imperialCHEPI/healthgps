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
#include <numeric>

#include "HealthGPS.Core/array2d.h"
#include "HealthGPS.Core/column.h"
#include "HealthGPS.Core/column_numeric.h"
#include "HealthGPS.Core/datatable.h"
#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/interval.h"
#include "HealthGPS.Input/datamanager.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/analysis_module.h"
#include "HealthGPS/api.h"
#include "HealthGPS/demographic.h"
#include "HealthGPS/disease.h"
#include "HealthGPS/event_aggregator.h"
#include "HealthGPS/gender_table.h"
#include "HealthGPS/lms_model.h"
#include "HealthGPS/modelinput.h"
#include "HealthGPS/monotonic_vector.h"
#include "HealthGPS/person.h"
#include "HealthGPS/riskfactor.h"
#include "HealthGPS/runtime_context.h"
#include "HealthGPS/scenario.h"
#include "HealthGPS/ses_noise_module.h"
#include "HealthGPS/static_linear_model.h"
#include "HealthGPS/two_step_value.h"
#include "HealthGPS/univariate_visitor.h"
#include "data_config.h"
#include "mock_repository.h"

using namespace hgps;
using namespace hgps::testing;

// Modified- Mahima
//  Forward declarations of helper functions
std::shared_ptr<ModelInput> create_test_modelinput();
Population
create_population(const std::shared_ptr<ModelInput>& input,
                  const std::map<SimulationModuleType, std::shared_ptr<SimulationModule>> &modules);

// Test event aggregator for mocking
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
    auto source = Disease{DiseaseStatus::active, start_year};
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
// Modified- Mahima
TEST(TestHealthGPS_Population, RegionModelParsing) {
    using namespace hgps;
    using namespace hgps::input;

    ASSERT_EQ(core::Region::England, parse_region("England"));
    ASSERT_EQ(core::Region::Wales, parse_region("Wales"));
    ASSERT_EQ(core::Region::Scotland, parse_region("Scotland"));
    ASSERT_EQ(core::Region::NorthernIreland, parse_region("NorthernIreland"));
    ASSERT_THROW(parse_region("Invalid"), core::HgpsException);
}
// Modified- Mahima
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
// Modified- Mahima
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

// Fix move operations for TestScenario
class TestScenario final : public Scenario {
  public:
    // Default constructor
    TestScenario() = default;
    ~TestScenario() override = default;

    // Copy operations - deleted
    TestScenario(const TestScenario &) = delete;
    TestScenario &operator=(const TestScenario &) = delete;

    // Move operations - need to be deleted because SyncChannel is not movable
    // NOLINTNEXTLINE(clang-diagnostic-defaulted-function-deleted)
    // Explicitly deleted because member channel_ has a deleted move constructor
    TestScenario(TestScenario &&) noexcept = delete;
    // NOLINTNEXTLINE(clang-diagnostic-defaulted-function-deleted)
    // Explicitly deleted because member channel_ has a deleted move assignment operator
    TestScenario &operator=(TestScenario &&) noexcept = delete;

    [[nodiscard]] ScenarioType type() noexcept override { return ScenarioType::baseline; }
    [[nodiscard]] std::string name() override { return "Test"; }
    [[nodiscard]] SyncChannel &channel() override { return channel_; }

    void clear() noexcept override {}
    [[nodiscard]] double apply([[maybe_unused]] Random &generator, [[maybe_unused]] Person &entity,
                               [[maybe_unused]] int time,
                               [[maybe_unused]] const core::Identifier &factor,
                               double value) override {
        return value;
    }

  private:
    // Removed redundant initializer
    // NOLINTNEXTLINE(readability-redundant-member-init)
    SyncChannel channel_; // Member without redundant initializer
};

// Standard implementation that doesn't need a DataManager
std::shared_ptr<ModelInput> create_test_modelinput() {
    // Create data table with required columns
    core::DataTable data;

    // Add region column and probabilities
    std::vector<std::string> region_data{"England", "Wales", "Scotland", "NorthernIreland"};
    auto region_col =
        std::make_unique<core::StringDataTableColumn>("region", std::move(region_data));
    data.add(std::move(region_col));

    std::vector<double> region_prob_data{0.5, 0.2, 0.2, 0.1};
    auto region_prob_col =
        std::make_unique<core::DoubleDataTableColumn>("region_prob", std::move(region_prob_data));
    data.add(std::move(region_prob_col));

    // Add ethnicity column and probabilities
    std::vector<std::string> ethnicity_data{"White", "Asian", "Black", "Others"};
    auto ethnicity_col =
        std::make_unique<core::StringDataTableColumn>("ethnicity", std::move(ethnicity_data));
    data.add(std::move(ethnicity_col));

    std::vector<double> ethnicity_prob_data{0.5, 0.25, 0.15, 0.1};
    auto ethnicity_prob_col = std::make_unique<core::DoubleDataTableColumn>(
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
    core::IntegerInterval age_range{1, 100};
    auto country =
        core::Country{826, std::string("United Kingdom"), std::string("GB"), std::string("GBR")};
    Settings settings(country, 1.0f, age_range);
    RunInfo run_info{};
    run_info.start_time = 2018;
    run_info.stop_time = 2025;
    run_info.sync_timeout_ms = 1000; // Default 1 second timeout
    run_info.seed = std::nullopt;
    run_info.verbosity = core::VerboseMode::none;
    run_info.comorbidities = 2; // Default value for max comorbidities
    SESDefinition ses_info{};
    ses_info.fuction_name = "linear";
    ses_info.parameters = {1.0};
    std::vector<MappingEntry> entries;
    entries.emplace_back("test", 0, std::nullopt);
    HierarchicalMapping risk_mapping{std::move(entries)};
    std::vector<core::DiseaseInfo> diseases;

    // NOLINTNEXTLINE(performance-move-const-arg)
    // run_info is a trivially-copyable type, so std::move has no effect and is removed
    auto model_input = std::make_shared<ModelInput>(std::move(data), std::move(settings),
                                                    run_info, std::move(ses_info),
                                                    std::move(risk_mapping), std::move(diseases));
    return model_input;
}

// Helper function to create a population with the provided modules
Population create_population(
    const std::shared_ptr<ModelInput>& input,
    const std::map<SimulationModuleType, std::shared_ptr<SimulationModule>> &modules) {
    // Initialize the runtime context
    auto bus = std::make_shared<TestEventAggregator>();
    auto scenario = std::make_unique<TestScenario>();

    // Verify input age range is valid
    assert(input != nullptr);
    assert(input->settings().age_range().lower() > 0); // Must be greater than zero
    assert(input->settings().age_range().lower() <
           input->settings().age_range().upper()); // Must be less than upper

    RuntimeContext context(bus, input, std::move(scenario));

    // Reset population with size 1 and set the current time to 60
    context.reset_population(1);
    context.set_current_time(60);

    // Initialize the population with each module
    for (const auto &[type, module] : modules) {
        assert(module != nullptr);
        module->initialise_population(context);
    }

    // Return a copy of the population from the context
    return context.population();
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
    // Set up variables explicitly to track their values
    int start_year = 2018;
    int end_year = 2025;

    // Create basic test model input
    auto input = create_test_modelinput();
    ASSERT_NE(nullptr, input);
    ASSERT_EQ(1, input->settings().age_range().lower());   // Verify age range lower is 1
    ASSERT_EQ(100, input->settings().age_range().upper()); // Verify age range upper is 100

    // Create simulation module map with mock implementations
    auto repo = std::make_shared<MockRepository>();
    ASSERT_NE(nullptr, repo);

    // Create modules map
    std::map<SimulationModuleType, std::shared_ptr<SimulationModule>> modules;

    // Create population data with valid age range for all years
    std::map<int, std::map<int, PopulationRecord>> pop_data;

    // Create a complete age range from 1 to 100 to match the model input for all years
    for (int year = start_year; year <= end_year; year++) {
        for (int age = 1; age <= 100; age++) {
            pop_data[year].emplace(age, PopulationRecord(age, 1000.0f, 1000.0f));
        }
    }

    // Create births data for all years
    std::map<int, Birth> births;
    for (int year = start_year; year <= end_year; year++) {
        births.emplace(year, Birth(200.0f, 105.0f));
    }

    // Create mortality data for all ages and all years
    std::map<int, std::map<int, Mortality>> deaths;
    for (int year = start_year; year <= end_year; year++) {
        for (int age = 1; age <= 100; age++) {
            deaths[year][age] = Mortality(0.01f, 0.01f);
        }
    }

    // Initialize runtime context first to verify there's no issue
    auto bus = std::make_shared<TestEventAggregator>();
    auto scenario = std::make_unique<TestScenario>();
    RuntimeContext context(bus, input, std::move(scenario));
    ASSERT_NO_THROW(context.age_range()); // Verify age range can be accessed

    // Create life table with complete age range
    auto life_table = LifeTable(std::move(births), std::move(deaths));

    // Verify life table time and age limits
    auto time_limits = life_table.time_limits();
    ASSERT_EQ(start_year, time_limits.lower());
    ASSERT_EQ(end_year, time_limits.upper());

    auto age_limits = life_table.age_limits();
    ASSERT_EQ(1, age_limits.lower());   // We start at age 1
    ASSERT_EQ(100, age_limits.upper()); // We end at age 100

    // Create demographic module
    auto demographic =
        std::make_shared<DemographicModule>(std::move(pop_data), std::move(life_table));
    ASSERT_NE(nullptr, demographic);

    // Add to modules map
    modules[SimulationModuleType::Demographic] = demographic;

    // Create risk factor models with required types (static and dynamic)
    std::map<RiskFactorModelType, std::unique_ptr<RiskFactorModel>> risk_models;

    // Create a mock static risk factor model
    // Fix unnamed parameters in MockStaticRiskFactorModel
    class MockStaticRiskFactorModel : public RiskFactorModel {
      public:
        RiskFactorModelType type() const noexcept override { return RiskFactorModelType::Static; }
        std::string name() const noexcept override { return "MockStatic"; }
        // Parameter name is commented out to avoid unreferenced parameter warning
        // while still satisfying clang-tidy named parameter requirement
        // NOLINTNEXTLINE(readability-named-parameter)
        void generate_risk_factors(RuntimeContext& /*context*/) override {}
        // NOLINTNEXTLINE(readability-named-parameter)
        void update_risk_factors(RuntimeContext& /*context*/) override {}
    };

    // Fix unnamed parameters in MockDynamicRiskFactorModel
    class MockDynamicRiskFactorModel : public RiskFactorModel {
      public:
        RiskFactorModelType type() const noexcept override { return RiskFactorModelType::Dynamic; }
        std::string name() const noexcept override { return "MockDynamic"; }
        // Parameter name is commented out to avoid unreferenced parameter warning
        // while still satisfying clang-tidy named parameter requirement
        // NOLINTNEXTLINE(readability-named-parameter)
        void generate_risk_factors(RuntimeContext& /*context*/) override {}
        // NOLINTNEXTLINE(readability-named-parameter)
        void update_risk_factors(RuntimeContext& /*context*/) override {}
    };

    // Add both required model types
    risk_models[RiskFactorModelType::Static] = std::make_unique<MockStaticRiskFactorModel>();
    risk_models[RiskFactorModelType::Dynamic] = std::make_unique<MockDynamicRiskFactorModel>();

    auto riskfactor = std::make_shared<RiskFactorModule>(std::move(risk_models));
    modules[SimulationModuleType::RiskFactor] = riskfactor;

    // Create minimal disease module with empty models
    std::map<core::Identifier, std::shared_ptr<DiseaseModel>> disease_models;
    auto disease = std::make_shared<DiseaseModule>(std::move(disease_models));
    modules[SimulationModuleType::Disease] = disease;

    // Create population and verify it succeeds
    auto population = create_population(input, modules);
    ASSERT_EQ(1, population.size());
}
