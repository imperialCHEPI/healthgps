#include "pch.h"
#include <atomic>
#include "HealthGPS\api.h"
#include "HealthGPS.Datastore\api.h"

namespace fs = std::filesystem;

struct Person : public hgps::Entity {

	Person(int id) : id_{id}{}

	int get_id() const override { return id_; }

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
	Scenario inputs(2010, 2030, 1);

	auto sim = HealthGPS(inputs, MTRandom32());
	EXPECT_TRUE(sim.next_double() >= 0);
}

TEST(TestHelathGPS, ModuleFactoryRegistry)
{
	using namespace hgps;
	using namespace hgps::data;

	auto full_path = fs::absolute("../../../data");

	auto manager = DataManager(full_path);

	auto factory = ModuleFactory(manager);
	factory.Register("country",
		[](hgps::core::Datastore& manager) -> ModuleFactory::ModuleType {
			return build_country_module(manager);
		});

	auto p = Person(5);

	auto country = factory.Create("country");
	country->execute("print", p);
	EXPECT_EQ(p.get_id(), 5);
}