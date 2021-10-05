#include "pch.h"
#include <atomic>
#include <map>

#include "HealthGPS/api.h"
#include "HealthGPS/event_bus.h"

#include "HealthGPS.Datastore\api.h"

#include "RiskFactorData.h"

namespace fs = std::filesystem;

void create_test_datatable(hgps::core::DataTable& data) {
	using namespace hgps;
	using namespace hgps::core;

	auto gender_values = std::vector<int>{ 1, 0, 0, 1, 0 };
	auto age_values = std::vector<int>{ 4, 9, 14, 19, 25 };
	auto edu_values = std::vector<float>{ 6.0f, 10.0f, 2.0f, 9.0f, 12.0f };
	auto inc_values = std::vector<double>{ 2.0, 10.0, 5.0, std::nan(""), 13.0 };

	auto gender_builder = IntegerDataTableColumnBuilder{ "Gender" };
	auto age_builder = IntegerDataTableColumnBuilder{ "Age" };
	auto edu_builder = FloatDataTableColumnBuilder{ "Education" };
	auto inc_builder = DoubleDataTableColumnBuilder{ "Income" };

	for (size_t i = 0; i < gender_values.size(); i++)
	{
		gender_builder.append(gender_values[i]);
		age_builder.append(age_values[i]);
		edu_builder.append(edu_values[i]);
		if (std::isnan(inc_values[i]))
			inc_builder.append_null();
		else
			inc_builder.append(inc_values[i]);
	}

	data.add(gender_builder.build());
	data.add(age_builder.build());
	data.add(edu_builder.build());
	data.add(inc_builder.build());
}

hgps::ModelInput create_test_configuration(hgps::core::DataTable& data) {
	using namespace hgps;
	using namespace hgps::core;

	auto uk = core::Country{ .code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR" };

	auto age_range = core::IntegerInterval(0, 30);
	auto settings = Settings(uk, 0.1f, "Age", age_range);
	auto info = RunInfo{ .start_time = 2018, .stop_time = 2025, .seed = std::nullopt };
	auto ses = SESMapping();
	ses.entries.emplace("gender", "Gender");
	ses.entries.emplace("age", "Age");
	ses.entries.emplace("education", "Education");
	ses.entries.emplace("income", "Income");

	auto entries = std::vector<MappingEntry>{
		MappingEntry("Year", 0, "", true),
		MappingEntry("Gender", 0, "gender"),
		MappingEntry("Age", 0, "age"),
		MappingEntry("SmokingStatus", 1),
		MappingEntry("AlcoholConsumption", 1),
		MappingEntry("BMI", 2)
	};

	auto mapping = HierarchicalMapping(std::move(entries));

	auto diseases = std::vector<core::DiseaseInfo>{
		DiseaseInfo{.code = "asthma", .name = "Asthma"},
		DiseaseInfo{.code = "diabetes", .name = "Diabetes Mellitus"},
		DiseaseInfo{.code = "lowbackpain", .name = "Low Back Pain"},
	};

	return ModelInput(data, settings, info, ses, mapping, diseases);
}

TEST(TestHealthGPS, RandomBitGenerator)
{
	using namespace hgps;

	MTRandom32 rnd;
	EXPECT_EQ(RandomBitGenerator::min(), MTRandom32::min());
	EXPECT_EQ(RandomBitGenerator::max(), MTRandom32::max());
	EXPECT_TRUE(rnd() >= 0);
}

TEST(TestHealthGPS, RandomBitGeneratorCopy)
{
	using namespace hgps;

	MTRandom32 rnd(123456789);
	auto copy = rnd;

	for (size_t i = 0; i < 10; i++)
	{
		EXPECT_EQ(rnd(), copy());
	}

	auto discard = rnd();
	EXPECT_NE(rnd(), copy());
}

TEST(TestHealthGPS, RandomBitGeneratorNextIntRangeIsClosed)
{
	using namespace hgps;

	auto rnd = MTRandom32{ 123456789 };

	auto summary_one = core::UnivariateSummary();
	auto summary_two = core::UnivariateSummary();

	auto sample_min = 1;
	auto sample_max = 10;
	auto sample_size = 100;
	for (size_t i = 0; i < sample_size; i++)
	{
		summary_one.append(rnd.next_int(sample_max));
		summary_two.append(rnd.next_int(sample_min, sample_max));
	}

	ASSERT_EQ(0.0, summary_one.min());
	ASSERT_EQ(sample_max, summary_one.max());

	ASSERT_EQ(sample_min, summary_two.min());
	ASSERT_EQ(sample_max, summary_two.max());
}

TEST(TestHealthGPS, RandomBitGeneratorEmpiricalDiscrete)
{
	using namespace hgps;

	auto rnd = MTRandom32{ 123456789 };

	auto values = std::vector<int>{2, 3, 5, 7, 9};
	auto freq_pdf = std::vector<float>{0.2f, 0.4f, 0.1f, 0.2f, 0.1f };
	auto cdf = std::vector<float>(freq_pdf.size());

	cdf[0] = freq_pdf[0];
	for (auto i = 1; i < freq_pdf.size(); i++) {
		cdf[i] = cdf[i - 1] + freq_pdf[i];
	}

	ASSERT_EQ(1.0, cdf.back());
	for (auto i = 1; i < freq_pdf.size(); i++) {
		ASSERT_LT(cdf[i - 1], cdf[i]);
	}

	auto summary = core::UnivariateSummary();
	auto sample_size = 100;
	for (size_t i = 0; i < sample_size; i++) {
		summary.append(rnd.next_empirical_discrete(values, cdf));
	}

	ASSERT_EQ(2.0, summary.min());
	ASSERT_EQ(9.0, summary.max());
}

TEST(TestHealthGPS, CreateRuntimeContext)
{
	using namespace hgps;
	using namespace hgps::data;

	DataTable data;
	create_test_datatable(data);

	auto bus = DefaultEventBus{};
	auto channel = SyncChannel{};
	auto rnd = MTRandom32{ 123456789 };
	auto scenario = BaselineScenario{ channel };
	auto config = create_test_configuration(data);
	auto definition = SimulationDefinition(config, std::move(scenario), std::move(rnd));

	auto context = RuntimeContext(bus, definition);
	ASSERT_EQ(0, context.population().size());
	ASSERT_EQ(0, context.time_now());
}

TEST(TestHealthGPS, SimulationInitialise)
{
	using namespace hgps;
	using namespace hgps::data;

	DataTable data;
	create_test_datatable(data);

	auto bus = DefaultEventBus{};
	auto channel = SyncChannel{};
	auto rnd = MTRandom32{ 123456789 };
	auto scenario = BaselineScenario{ channel };
	auto config = create_test_configuration(data);
	auto definition = SimulationDefinition(config, std::move(scenario), std::move(rnd));

	auto full_path = fs::absolute("../../../data");
	auto manager = DataManager(full_path);
	auto repository = CachedRepository(manager);

	auto baseline_data = hgps::BaselineAdjustment{};
	repository.register_linear_model_definition(HierarchicalModelType::Static, get_static_test_model(baseline_data));
	repository.register_linear_model_definition(HierarchicalModelType::Dynamic, get_dynamic_test_model(baseline_data));

	auto factory = get_default_simulation_module_factory(repository);
	ASSERT_NO_THROW(HealthGPS(std::move(definition), factory, bus));
}

TEST(TestHealthGPS, ModuleFactoryRegistry)
{
	using namespace hgps;
	using namespace hgps::data;

	auto count = 10U;
	auto builder = core::FloatDataTableColumnBuilder("Test");
	for (size_t i = 0; i < count; i++) {
		if ((i % 2) == 0)
			builder.append(i * 2.5f);
		else
			builder.append_null();
	}

	auto data = core::DataTable();
	data.add(builder.build());

	auto uk = core::Country{ .code = 826, .name = "United Kingdom", .alpha2 = "GB", .alpha3 = "GBR" };
	auto age_range = core::IntegerInterval(0, 100);
	auto settings = Settings(uk, 0.1f, "Age", age_range);
	auto info = RunInfo{ .start_time = 1, .stop_time = count, .seed = std::nullopt };
	auto ses = SESMapping();
	ses.entries.emplace("test", builder.name());

	auto mapping = HierarchicalMapping(
		std::vector<MappingEntry>{
			MappingEntry("Year", 0),
			MappingEntry("Gender", 0, "gender"),
			MappingEntry("Age", 0, "age"),
	});

	auto diseases = std::vector<core::DiseaseInfo>{
		DiseaseInfo{.code = "angina", .name = "Angina Pectoris"},
		DiseaseInfo{.code = "diabetes", .name = "Diabetes Mellitus"}
	};

	auto config = ModelInput(data, settings, info, ses, mapping, diseases);

	auto full_path = fs::absolute("../../../data");
	auto manager = DataManager(full_path);
	auto repository = CachedRepository(manager);

	auto factory = SimulationModuleFactory(repository);
	factory.register_builder(SimulationModuleType::Simulator,
		[](Repository& repository, const ModelInput& config) -> SimulationModuleFactory::ModuleType {
			return build_country_module(repository, config);
		});

	auto base_module = factory.create(SimulationModuleType::Simulator, config);
	auto country_mod = static_cast<CountryModule*>(base_module.get());
	country_mod->execute("print");

	ASSERT_EQ(SimulationModuleType::Simulator, country_mod->type());
	ASSERT_EQ("Country", country_mod->name());
}

TEST(TestHealthGPS, CreateSESModule)
{
	using namespace hgps;
	using namespace hgps::data;

	DataTable data;
	create_test_datatable(data);

	auto config = create_test_configuration(data);

	auto full_path = fs::absolute("../../../data");
	auto manager = DataManager(full_path);
	auto repository = CachedRepository(manager);

	auto ses_module = build_ses_module(repository, config);
	auto edu_hist = ses_module->get_education_frequency(std::nullopt);
	auto edu_size = ses_module->max_education_level() + 1;
	auto inc_hist = ses_module->get_income_frenquency(std::nullopt);
	auto inc_size = edu_size * (ses_module->max_incoming_level() + 1);

	ASSERT_EQ(SimulationModuleType::SES, ses_module->type());
	ASSERT_EQ("SES", ses_module->name());
	ASSERT_EQ(12, ses_module->max_education_level());
	ASSERT_EQ(13, ses_module->max_incoming_level());
	ASSERT_EQ(5, ses_module->data().size());
	ASSERT_EQ(edu_size, edu_hist.begin()->second.size());
	ASSERT_EQ(inc_size, inc_hist.begin()->second.size());
}

TEST(TestHealthGPS, CreateDemographicModule)
{
	using namespace hgps;
	using namespace hgps::data;

	DataTable data;
	create_test_datatable(data);

	auto config = create_test_configuration(data);

	auto full_path = fs::absolute("../../../data");
	auto manager = DataManager(full_path);
	auto repository = CachedRepository(manager);

	auto pop_module = build_demographic_module(repository, config);
	auto total_pop = pop_module->get_total_population(config.start_time());
	auto pop_dist = pop_module->get_age_gender_distribution(config.start_time());
	auto birth_rate = pop_module->get_birth_rate(config.start_time());
	auto sum_male = 0.0;
	auto sum_female = 0.0;
	for (auto& age : pop_dist) {
		sum_male += age.second.male;
		sum_female += age.second.female;
	}
	auto sum_prob = sum_male + sum_female;
	ASSERT_EQ(SimulationModuleType::Demographic, pop_module->type());
	ASSERT_EQ("Demographic", pop_module->name());
	ASSERT_GT(total_pop, 0);
	ASSERT_GT(pop_dist.size(), 0);
	ASSERT_GT(birth_rate.male, 0);
	ASSERT_GT(birth_rate.female, 0);
	ASSERT_TRUE((sum_prob - 1.0) < 1e-4);
}

TEST(TestHealthGPS, CreateRiskFactorModule)
{
	using namespace hgps;
	using namespace hgps::data;

	// Test data code generation via JSON model definition.
	/*
	auto static_code = generate_test_code(
		HierarchicalModelType::Static, "C:/HealthGPS/Test/HLM.Json");

	auto dynamic_code = generate_test_code(
		HierarchicalModelType::Dynamic, "C:/HealthGPS/Test/DHLM.Json");
	*/

	auto baseline_data = hgps::BaselineAdjustment{};
	auto static_definition = get_static_test_model(baseline_data);
	auto dynamic_definion = get_dynamic_test_model(baseline_data);
	auto risk_models = std::unordered_map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>();
	risk_models.emplace(HierarchicalModelType::Static, std::make_unique<StaticHierarchicalLinearModel>(static_definition));
	risk_models.emplace(HierarchicalModelType::Dynamic, std::make_unique<DynamicHierarchicalLinearModel>(dynamic_definion));

	auto risk_module = RiskFactorModule{ std::move(risk_models) };

	ASSERT_EQ(SimulationModuleType::RiskFactor, risk_module.type());
	ASSERT_EQ("RiskFactor", risk_module.name());
}

TEST(TestHealthGPS, CreateRiskFactorModuleFailWithEmpty)
{
	using namespace hgps;
	auto risk_models = std::unordered_map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>();
	ASSERT_THROW(auto x = RiskFactorModule(std::move(risk_models)), std::invalid_argument);
}

TEST(TestHealthGPS, CreateRiskFactorModuleFailWithoutStatic)
{
	using namespace hgps;

	auto baseline_data = hgps::BaselineAdjustment{};
	auto dynamic_definion = get_dynamic_test_model(baseline_data);
	auto risk_models = std::unordered_map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>();
	risk_models.emplace(HierarchicalModelType::Dynamic, std::make_unique<DynamicHierarchicalLinearModel>(dynamic_definion));

	ASSERT_THROW(auto x = RiskFactorModule(std::move(risk_models)), std::invalid_argument);
}

TEST(TestHealthGPS, CreateRiskFactorModuleFailWithoutDynamic)
{
	using namespace hgps;

	auto baseline_data = hgps::BaselineAdjustment{};
	auto static_definition = get_static_test_model(baseline_data);
	auto risk_models = std::unordered_map<HierarchicalModelType, std::unique_ptr<HierarchicalLinearModel>>();
	risk_models.emplace(HierarchicalModelType::Static, std::make_unique<StaticHierarchicalLinearModel>(static_definition));

	ASSERT_THROW(auto x = RiskFactorModule(std::move(risk_models)), std::invalid_argument);
}

TEST(TestHealthGPS, CreateDiseaseModule)
{
	using namespace hgps;
	using namespace hgps::data;

	DataTable data;
	create_test_datatable(data);

	auto full_path = fs::absolute("../../../data");
	auto manager = DataManager(full_path);
	auto repository = CachedRepository(manager);

	auto inputs = create_test_configuration(data);
	auto test_person = Person{};
	test_person.age = 50;
	test_person.gender = core::Gender::male;

	auto disease_module = build_disease_module(repository, inputs);
	ASSERT_EQ(SimulationModuleType::Disease, disease_module->type());
	ASSERT_EQ("Disease", disease_module->name());
	ASSERT_GT(disease_module->size(), 0);
	ASSERT_TRUE(disease_module->contains("diabetes"));
	ASSERT_FALSE(disease_module->contains("moonshot"));
	ASSERT_GT(disease_module->get_excess_mortality("diabetes", test_person), 0);
	ASSERT_EQ(0.0, disease_module->get_excess_mortality("moonshot", test_person));
}

TEST(TestHealthGPS, CreateAnalysisModule)
{
	using namespace hgps;
	using namespace hgps::data;

	DataTable data;
	create_test_datatable(data);

	auto full_path = fs::absolute("../../../data");
	auto manager = DataManager(full_path);
	auto repository = CachedRepository(manager);

	auto inputs = create_test_configuration(data);

	auto analysis_module = build_analysis_module(repository, inputs);
	ASSERT_EQ(SimulationModuleType::Analysis, analysis_module->type());
	ASSERT_EQ("Analysis", analysis_module->name());
}