#include "pch.h"
#include <atomic>
#include "HealthGPS\api.h"
#include "HealthGPS.Datastore\api.h"

namespace fs = std::filesystem;

struct Person : public hgps::Entity {

	Person(int id) : id_{id}{}

	int id() const override { return id_; }

	virtual std::string to_string() const override {
		return std::format("Person # {}", id_);
	}

private:
	int id_;
};

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


TEST(TestHealthGPS, SimulationInitialise)
{
	using namespace hgps;

	auto count = 10U;
	auto uk = core::Country{ .code = "GB", .name = "United Kingdom" };
	auto builder = core::FloatDataTableColumnBuilder("Test");
	for (size_t i = 0; i < count; i++)
	{
		if ((i % 2) == 0)
			builder.append(i * 2.5f);
		else
			builder.append_null();
	}

	auto data = core::DataTable();
	data.add(builder.build());
	auto age_range = core::IntegerInterval(0, 110);
	auto pop = Population(uk, builder.name(), 1, 0.1f, "AgeGroup", age_range);
	auto info = RunInfo{ .start_time = 1, .stop_time = count, .seed = std::nullopt };
	auto ses = SESMapping();
	ses.entries.emplace("test", builder.name());
	auto context = ModelContext(data, pop, info, ses);

	auto sim = HealthGPS(context, MTRandom32());
	EXPECT_TRUE(sim.next_double() >= 0);
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

	auto uk = core::Country{ .code = "GB", .name = "United Kingdom" };
	auto age_range = core::IntegerInterval(0, 110);
	auto pop = Population(uk, builder.name(), 1, 0.1f, "AgeGroup", age_range);
	auto info = RunInfo{ .start_time = 1, .stop_time = count, .seed = std::nullopt };
	auto ses = SESMapping();
	ses.entries.emplace("test", builder.name());
	auto context = ModelContext(data, pop, info, ses);

	auto full_path = fs::absolute("../../../data");
	auto manager = DataManager(full_path);

	auto factory = SimulationModuleFactory(manager);
	factory.Register(SimulationModuleType::Simulator,
		[](core::Datastore& manager, ModelContext& context) -> SimulationModuleFactory::ModuleType {
			return build_country_module(manager, context);
		});

	auto p = Person(5);

	MTRandom32 rnd;
	auto country_mod = factory.Create(SimulationModuleType::Simulator, context);
	country_mod->execute("print", rnd, p);
	EXPECT_EQ(p.id(), 5);
}

TEST(TestHealthGPS, CreateSESModule)
{
	using namespace hgps;
	using namespace hgps::data;
	using namespace hgps::core;

	auto gender_values = std::vector<int>{1, 0, 0, 1, 0 };
	auto age_values = std::vector<std::string>{ "0-4", "5-9", "10-14", "15-19", "20-25" };
	auto edu_values = std::vector<float>{ 6.0f, 10.0f, 2.0f, 9.0f, 12.0f };
	auto inc_values = std::vector<double>{ 2.0, 10.0, 5.0, std::nan(""), 13.0};

	auto gender_builder = IntegerDataTableColumnBuilder{ "Gender" };
	auto age_builder = StringDataTableColumnBuilder{ "AgeGroup" };
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

	auto data = DataTable();
	data.add(gender_builder.build());
	data.add(age_builder.build());
	data.add(edu_builder.build());
	data.add(inc_builder.build());

	auto uk = core::Country{ .code = "GB", .name = "United Kingdom" };
	auto age_range = core::IntegerInterval(0, 30);
	auto pop = Population(uk, "AgeGroup", 1, 0.1f, "AgeGroup", age_range);
	auto info = RunInfo{ .start_time = 1, .stop_time = 10, .seed = std::nullopt };
	auto ses = SESMapping();
	ses.entries.emplace("gender", gender_builder.name());
	ses.entries.emplace("age_group", age_builder.name());
	ses.entries.emplace("education", edu_builder.name());
	ses.entries.emplace("income", inc_builder.name());
	auto context = ModelContext(data, pop, info, ses);

	auto full_path = fs::absolute("../../../data");
	auto manager = DataManager(full_path);

	auto ses_module = build_ses_module(manager, context);
	auto pub_hist = ses_module->get_education_frequency(std::nullopt);
	auto inc_hist = ses_module->get_income_frenquency(std::nullopt);

	ASSERT_EQ(SimulationModuleType::SES, ses_module->type());
	ASSERT_EQ("SES", ses_module->name());
	ASSERT_EQ(12, ses_module->max_education_level());
	ASSERT_EQ(13, ses_module->max_incoming_level());
	ASSERT_EQ(5, ses_module->data().size());
}