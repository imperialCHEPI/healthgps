#ifdef _MSC_VER
#pragma warning(disable : 26439) // This kind of function should not throw. Declare it 'noexcept'
#endif

#include "pch.h"
#include <gtest/gtest.h>
#include <memory>

#include "HealthGPS.Core/column.h"
#include "HealthGPS.Core/datatable.h"
#include "HealthGPS.Input/model_parser.h"
#include "HealthGPS/api.h"
#include "HealthGPS/event_aggregator.h"
#include "HealthGPS/modelinput.h"
#include "HealthGPS/person.h"
#include "HealthGPS/runtime_context.h"
#include "HealthGPS/scenario.h"
#include "HealthGPS/static_linear_model.h"
#include "HealthGPS/two_step_value.h"
using namespace hgps;

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

    auto time_now = 2022u;
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

    auto time_now = 2022u;
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

    auto v = TwoStepValue<int>{5};
    ASSERT_EQ(5, v());
    ASSERT_EQ(5, v.value());
    ASSERT_EQ(0, v.old_value());
}

TEST(TestHealthGPS_Population, AssignToTwoStepValue) {
    using namespace hgps;

    auto v = TwoStepValue<int>{5};
    v = 10;
    v.set_value(v.value() + 3);

    ASSERT_EQ(13, v());
    ASSERT_EQ(13, v.value());
    ASSERT_EQ(10, v.old_value());
}

TEST(TestHealthGPS_Population, SetBothTwoStepValues) {
    using namespace hgps;

    auto v = TwoStepValue<int>{};
    v.set_both_values(15);

    ASSERT_EQ(15, v());
    ASSERT_EQ(15, v.value());
    ASSERT_EQ(15, v.old_value());
}

TEST(TestHealthGPS_Population, CloneTwoStepValues) {
    using namespace hgps;

    auto source = TwoStepValue<int>{5};
    source = 10;

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

    auto source = Disease{.status = DiseaseStatus::active, .start_time = 2021};
    auto clone = source.clone();

    ASSERT_EQ(source.status, clone.status);
    ASSERT_EQ(source.start_time, clone.start_time);
    ASSERT_NE(std::addressof(source), std::addressof(clone));
}

TEST(TestHealthGPS_Population, AddSingleNewEntity) {
    using namespace hgps;

    constexpr auto init_size = 10;

    auto p = Population{init_size};
    ASSERT_EQ(p.initial_size(), p.current_active_size());

    auto time_now = 2022;
    auto start_size = p.size();
    p.add(Person{core::Gender::male}, time_now);
    ASSERT_GT(p.size(), start_size);

    p[start_size].die(time_now);
    ASSERT_FALSE(p[start_size].is_active());

    time_now++;
    auto current_size = p.size();
    p.add(Person{core::Gender::female}, time_now);
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

// Add this test implementation:
class TestScenario : public Scenario {
  public:
    ScenarioType type() noexcept override { return ScenarioType::baseline; }
    std::string name() override { return "Test"; }
    SyncChannel &channel() override { return channel_; }
    void clear() noexcept override {}
    double apply([[maybe_unused]] Random &generator, [[maybe_unused]] Person &entity,
                 [[maybe_unused]] int time,
                 [[maybe_unused]] const core::Identifier &risk_factor_key, double value) override {
        return value;
    }

  private:
    SyncChannel channel_;
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
    class TestEventAggregator : public EventAggregator {
      public:
        void publish(std::unique_ptr<EventMessage> /*message*/) override {}
        void publish_async(std::unique_ptr<EventMessage> /*message*/) override {}
        std::unique_ptr<EventSubscriber>
        subscribe(EventType /*event_id*/,
                  std::function<void(std::shared_ptr<EventMessage>)> /*handler*/) override {
            return nullptr;
        }
        bool unsubscribe(const EventSubscriber & /*subscriber*/) override { return true; }
    };

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
    class TestEventAggregator : public EventAggregator {
      public:
        void publish(std::unique_ptr<EventMessage> /*message*/) override {}
        void publish_async(std::unique_ptr<EventMessage> /*message*/) override {}
        std::unique_ptr<EventSubscriber>
        subscribe(EventType /*event_id*/,
                  std::function<void(std::shared_ptr<EventMessage>)> /*handler*/) override {
            return nullptr;
        }
        bool unsubscribe(const EventSubscriber & /*subscriber*/) override { return true; }
    };

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
