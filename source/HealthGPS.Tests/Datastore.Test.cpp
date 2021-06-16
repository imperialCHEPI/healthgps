#include "pch.h"
#include "HealthGPS.Datastore\api.h"

namespace fs = std::filesystem;

static auto store_full_path = fs::absolute("../../../data");
TEST(TestDatastore, CreateDataManager)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto countries = manager.get_countries();

	ASSERT_GT(countries.size(), 0);
}

TEST(TestDatastore, CountryIsCaseInsensitive)
{
	using namespace hgps::data;

	auto manager = DataManager(store_full_path);

	auto countries = manager.get_countries();
	auto gb_lower = manager.get_country("gb");
	auto gb_upper = manager.get_country("GB");

	ASSERT_GT(countries.size(), 0);
	ASSERT_TRUE(gb_lower.has_value());
	ASSERT_TRUE(gb_upper.has_value());
}

TEST(TestDatastore, CreateTableColumnWithNulls)
{
	using namespace hgps::data;

	auto str_col = StringDataTableColumn("string", { "Cat", "Dog", "" }, { true, true, false });
	auto flt_col = FloatDataTableColumn("float", { 5.7f, 15.37f, 0.0f, 20.75f }, { true, true, false, true});
	auto dbl_col = DoubleDataTableColumn("double", { 7.13, 15.37, 20.75 }, {true, true, true });
	auto int_col = IntegerDataTableColumn("integer", { 0, 15, 200 }, {false, true, true });

	ASSERT_EQ(3, str_col.length());
	ASSERT_EQ(1, str_col.null_count());
	ASSERT_EQ("Dog", *str_col.value(1));
	ASSERT_TRUE(str_col.is_null(2));
	ASSERT_FALSE(str_col.is_valid(2));
	ASSERT_FALSE(str_col.is_null(0));
	ASSERT_TRUE(str_col.is_valid(0));

	ASSERT_EQ(4, flt_col.length());
	ASSERT_EQ(1, flt_col.null_count());
	ASSERT_EQ(15.37f, *flt_col.value(1));
	ASSERT_TRUE(flt_col.is_null(2));
	ASSERT_FALSE(flt_col.is_valid(2));
	ASSERT_FALSE(flt_col.is_null(0));
	ASSERT_TRUE(flt_col.is_valid(0));

	ASSERT_EQ(3, dbl_col.length());
	ASSERT_EQ(0, dbl_col.null_count());
	ASSERT_EQ(15.37, *dbl_col.value(1));
	ASSERT_FALSE(dbl_col.is_null(1));
	ASSERT_TRUE(dbl_col.is_valid(1));

	ASSERT_EQ(3, int_col.length());
	ASSERT_EQ(1, int_col.null_count());
	ASSERT_EQ(15, *int_col.value(1));
	ASSERT_TRUE(int_col.is_null(0));
	ASSERT_FALSE(int_col.is_valid(0));
	ASSERT_FALSE(int_col.is_null(1));
	ASSERT_TRUE(int_col.is_valid(1));
}

TEST(TestDatastore, CreateTableColumnWithoutNulls)
{
	using namespace hgps::data;

	auto str_col = StringDataTableColumn("string", { "Cat", "Dog", "Cow" });
	auto flt_col = FloatDataTableColumn("float", { 7.13f, 15.37f, 0.0f, 20.75f });
	auto dbl_col = DoubleDataTableColumn("double", { 7.13, 15.37, 20.75 });
	auto int_col = IntegerDataTableColumn("integer", { 0, 15, 200 });

	ASSERT_EQ(3, str_col.length());
	ASSERT_EQ(0, str_col.null_count());
	ASSERT_EQ("Dog", *str_col.value(1));
	ASSERT_TRUE(str_col.is_valid(0));
	ASSERT_FALSE(str_col.is_null(0));

	ASSERT_EQ(4, flt_col.length());
	ASSERT_EQ(0, flt_col.null_count());
	ASSERT_EQ(15.37f, *flt_col.value(1));
	ASSERT_TRUE(flt_col.is_valid(0));
	ASSERT_FALSE(flt_col.is_null(0));

	ASSERT_EQ(3, dbl_col.length());
	ASSERT_EQ(0, dbl_col.null_count());
	ASSERT_EQ(15.37, *dbl_col.value(1));
	ASSERT_TRUE(dbl_col.is_valid(1));
	ASSERT_FALSE(dbl_col.is_null(1));

	ASSERT_EQ(3, int_col.length());
	ASSERT_EQ(0, int_col.null_count());
	ASSERT_EQ(15, *int_col.value(1));
	ASSERT_TRUE(int_col.is_valid(0));
	ASSERT_FALSE(int_col.is_null(0));
}

TEST(TestDatastore, CreateTableColumnFailWithLenMismatch)
{
	using namespace hgps::data;

	ASSERT_THROW(
		IntegerDataTableColumn("integer", { 5, 15, 0, 20 }, { true, false, true }),
		std::invalid_argument);
}

TEST(TestDatastore, CreateTableColumnFailWithShortName)
{
	using namespace hgps::data;

	ASSERT_THROW(
		IntegerDataTableColumn("f", {15, 0, 20 }, { true, false, true }),
		std::invalid_argument);
}

TEST(TestDatastore, CreateTableColumnFailWithInvalidName)
{
	using namespace hgps::data;

	ASSERT_THROW(
		IntegerDataTableColumn("5nteger", { 15, 0, 20 }, { true, false, true }),
		std::invalid_argument);
}

TEST(TestDatastore, TableColumnIterator)
{
	using namespace hgps::data;

	auto dbl_col = DoubleDataTableColumn("double",
		{ 1.5, 3.5, 2.0, 0.0, 3.0, 0.0, 5.0 },
		{ true, true, true, false, true, false, true });

	double loop_sum = 0.0;
	for (auto& item : dbl_col)
	{
		loop_sum += item.value_or(0.0);
	}

	auto sum = std::accumulate(dbl_col.begin(), dbl_col.end(), 0.0,
		[](auto a, auto b) {return b.has_value() ? a + b.value() : a; });

	ASSERT_EQ(7, dbl_col.length());
	ASSERT_EQ(2, dbl_col.null_count());
	ASSERT_EQ(15.0, sum);
	ASSERT_EQ(loop_sum, sum);
}

TEST(TestDatastore, CreateDataTable)
{
	using namespace hgps::data;

	auto str_values = std::vector<std::string>{ "Cat", "Dog", "", "Cow", "Fox"};
	auto flt_values = std::vector<float>{ 5.7f, 7.13f, 15.37f, 0.0f, 20.75f };
	auto int_values = std::vector<int>{ 15, 78, 154, 0, 200 };

	auto str_duilder = StringDataTableColumnBuilder{ "String" };
	auto ftl_duilder = FloatDataTableColumnBuilder{ "Floats" };
	auto dbl_duilder = DoubleDataTableColumnBuilder{ "Doubles" };
	auto int_duilder = IntegerDataTableColumnBuilder{ "Integer" };

	for (size_t i = 0; i < flt_values.size(); i++)
	{
		str_values[i].empty() ? str_duilder.append_null() : str_duilder.append(str_values[i]);
		flt_values[i] == float{} ? ftl_duilder.append_null() : ftl_duilder.append(flt_values[i]);
		flt_values[i] == double{} ? dbl_duilder.append_null() : dbl_duilder.append(flt_values[i] + 1.0);
		int_values[i] == int{} ? int_duilder.append_null() : int_duilder.append(int_values[i]);
	}

	auto table = DataTable();
	table.add(str_duilder.build());
	table.add(ftl_duilder.build());
	table.add(dbl_duilder.build());
	table.add(int_duilder.build());

	ASSERT_EQ(4, table.num_columns());
	ASSERT_EQ(5, table.num_rows());
}

TEST(TestDatastore, DataTableFailWithColumnLenMismath)
{
	using namespace hgps::data;

	auto flt_values = std::vector<float>{ 7.13f, 15.37f, 0.0f, 20.75f };
	auto int_values = std::vector<int>{ 154, 0, 200 };

	auto table = DataTable();
	table.add(std::make_unique<FloatDataTableColumn>(
		"float",
		std::move(flt_values),
		std::vector<bool> {true, true, false, true}));

	ASSERT_THROW(
	table.add(std::make_unique<IntegerDataTableColumn>(
		"integer",
		std::move(int_values),
		std::vector<bool> {true, false, true})),
		std::invalid_argument);
}

TEST(TestDatastore, DataTableFailDuplicateColumn)
{
	using namespace hgps::data;

	auto flt_values = std::vector<float>{7.13f, 15.37f, 0.0f, 20.75f };
	auto int_values = std::vector<int>{ 100, 154, 0, 200 };

	auto table = DataTable();
	table.add(std::make_unique<FloatDataTableColumn>(
		"Number",
		std::move(flt_values),
		std::vector<bool> {true, true, false, true}));

	ASSERT_THROW(
		table.add(std::make_unique<IntegerDataTableColumn>(
			"number",
			std::move(int_values),
			std::vector<bool> {true, true, false, true})),
 std::invalid_argument);
}