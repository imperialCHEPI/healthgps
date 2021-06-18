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

TEST(TestHelathGPS, RandomBitGenerator)
{
	using namespace hgps;

	MTRandom32 rnd;
	EXPECT_EQ(RandomBitGenerator::min(), MTRandom32::min());
	EXPECT_EQ(RandomBitGenerator::max(), MTRandom32::max());
	EXPECT_TRUE(rnd() >= 0);
}

TEST(TestHelathGPS, RandomBitGeneratorCopy)
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


TEST(TestHelathGPS, SimulationInitialise)
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
	auto pop = Population(uk, builder.name(), 1, 0.1);
	auto info = RunInfo{ .start_time = 1, .stop_time = count, .seed = std::nullopt };
	auto ses = SESMapping();
	ses.entries.emplace("test", builder.name());
	auto context = ModelContext(data, pop, info, ses);

	auto sim = HealthGPS(context, MTRandom32());
	EXPECT_TRUE(sim.next_double() >= 0);
}

TEST(TestHelathGPS, ModuleFactoryRegistry)
{
	using namespace hgps;
	using namespace hgps::data;

	auto full_path = fs::absolute("../../../data");

	auto manager = DataManager(full_path);
	auto country = manager.get_country("GB");

	auto factory = SimulationModuleFactory(manager);
	factory.Register(SimulationModuleType::Simulator,
		[](core::Datastore& manager, core::Country& country) -> SimulationModuleFactory::ModuleType {
			return build_country_module(manager, country);
		});

	auto p = Person(5);

	MTRandom32 rnd;
	auto country_mod = factory.Create(SimulationModuleType::Simulator, country.value());
	country_mod->execute("print", rnd, p);
	EXPECT_EQ(p.id(), 5);
}