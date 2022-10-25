#include "pch.h"

#include "HealthGPS.Core/identifier.h"

#include <unordered_map>
#include<sstream>

TEST(TestCore_Identity, CreateEmpty)
{
	using namespace hgps::core;
	auto empty = Identifier::empty();
	auto other = Identifier{};

	ASSERT_TRUE(empty.is_empty());
	ASSERT_TRUE(other.is_empty());

	ASSERT_EQ(0, empty.size());
	ASSERT_EQ(0, other.size());
	ASSERT_EQ(other, empty);
}

TEST(TestCore_Identity, CreateValid)
{
	using namespace hgps::core;
	auto str = std::string{ "TEST_5" };
	auto key = Identifier{str};

	ASSERT_FALSE(key.is_empty());
	ASSERT_EQ(str.size(), key.size());
	ASSERT_EQ("test_5", key.to_string());
}

TEST(TestCore_Identity, Equality)
{
	using namespace hgps::core;

	auto dodo = Identifier{ "Dodo" };
	auto dodo_lower = Identifier{ "dodo" };
	auto dodo_upper = Identifier{ "DODO" };
	auto duck = Identifier{ "Duck" };

	ASSERT_EQ(dodo, dodo);
	ASSERT_EQ(dodo, dodo_lower);
	ASSERT_EQ(dodo, dodo_upper);
	ASSERT_EQ(dodo_lower, dodo_upper);
	ASSERT_EQ(dodo.hash(), dodo_upper.hash());

	ASSERT_TRUE(dodo == dodo_lower);
	ASSERT_TRUE(dodo_lower == dodo_upper);
	ASSERT_TRUE(dodo != duck);
	ASSERT_FALSE(dodo == duck);
}

TEST(TestCore_Identity, Comparable)
{
	using namespace hgps::core;

	auto cat = Identifier{ "Cat" };
	auto dog = Identifier{ "DOG" };
	auto cow = Identifier{ "cat" };

	ASSERT_GT(dog, cat);
	ASSERT_LT(cow, dog);
	ASSERT_EQ(cat, cow);
	ASSERT_TRUE(dog > cat);
	ASSERT_TRUE(cow < dog);
	ASSERT_TRUE(dog >= cat);
	ASSERT_TRUE(cow <= dog);

	ASSERT_TRUE(cat >= cow);
	ASSERT_TRUE(cow >= cat);
	ASSERT_FALSE(cat > dog);
	ASSERT_FALSE(dog < cow);
}

TEST(TestCore_Identity, CreateInvalidThrow)
{
	using namespace hgps::core;
	ASSERT_THROW(Identifier{ "" }, std::invalid_argument);
	ASSERT_THROW(Identifier{ "5Start" }, std::invalid_argument);
	ASSERT_THROW(Identifier{ "With Space" }, std::invalid_argument);
	ASSERT_THROW(Identifier{ "With-10" }, std::invalid_argument);
	ASSERT_THROW(Identifier{ "With.dot" }, std::invalid_argument);
}

TEST(TestCore_Identity, CreateMapKey)
{
	using namespace hgps::core;

	auto cat = Identifier{ "Cat" };
	auto dog = Identifier{ "DOG" };
	auto cow = Identifier{ "cow" };
	auto tmp = Identifier{ "None" };

	auto animals = std::map<Identifier, int>{ {cat, 3}, {dog, 7}, {cow, 2} };

	ASSERT_EQ(3, animals.size());
	ASSERT_TRUE(animals.contains(cat));
	ASSERT_TRUE(animals.contains(dog));
	ASSERT_TRUE(animals.contains(cow));
	ASSERT_FALSE(animals.contains(tmp));
}

TEST(TestCore_Identity, CreateUnorderedMapKey)
{
	using namespace hgps::core;

	auto cat = Identifier{ "Cat" };
	auto dog = Identifier{ "DOG" };
	auto cow = Identifier{ "cow" };
	auto tmp = Identifier{ "None" };

	auto animals = std::unordered_map<Identifier, int> { {cat, 3}, {dog, 7}, {cow, 2} };

	ASSERT_EQ(3, animals.size());
	ASSERT_TRUE(animals.contains(cat));
	ASSERT_TRUE(animals.contains(dog));
	ASSERT_TRUE(animals.contains(cow));
	ASSERT_FALSE(animals.contains(tmp));
}

TEST(TestCore_Identity, ConvertToStrem)
{
	using namespace hgps::core;

	auto dodo = Identifier{ "Dodo_and_Duck" };

	std::stringstream stream;

	stream << dodo;
	auto dodo_str = stream.str();
	ASSERT_EQ(dodo.to_string(), dodo_str);
}