#include "pch.h"
#include "HealthGPS.Datastore\api.h"

namespace fs = std::filesystem;

TEST(TestDatastore, CreateDataManager)
{
	using namespace hgps::data;

	auto full_path = fs::absolute("../../../data");

	auto manager = DataManager(full_path);

	auto countries = manager.get_countries();

	ASSERT_GT(countries.size(), 0);
}

TEST(TestDatastore, CreateTableColumn)
{
	using namespace hgps::data;

	auto flt_col = FloatDataTableColumn("float", { 5.7f, 15.37f, 0.0f, 20.75f }, { true, true, false, true});
	auto dbl_col = DoubleDataTableColumn("double", { 7.13, 15.37, 20.75 }, {true, true, true });
	auto int_col = IntegerDataTableColumn("integer", { 0, 15, 200 }, {false, true, true });

	ASSERT_EQ(4, flt_col.length());
	ASSERT_EQ(1, flt_col.null_count());
	ASSERT_TRUE(flt_col.is_null(2));
	ASSERT_FALSE(flt_col.is_null(0));
	ASSERT_TRUE(flt_col.is_null(7)); // outside bound

	ASSERT_EQ(3, dbl_col.length());
	ASSERT_EQ(0, dbl_col.null_count());
	ASSERT_FALSE(dbl_col.is_null(1));
	ASSERT_TRUE(dbl_col.is_null(7)); // outside bound

	ASSERT_EQ(3, int_col.length());
	ASSERT_EQ(1, int_col.null_count());
	ASSERT_TRUE(int_col.is_null(0));
	ASSERT_FALSE(int_col.is_null(1));
	ASSERT_TRUE(int_col.is_null(7)); // outside bound
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

TEST(TestDatastore, TableColumnInterator)
{
	using namespace hgps::data;

	auto dbl_col = DoubleDataTableColumn("double", { 1.5, 3.5, 2.0, 3.0 }, { true, true, true, true });

	auto sum = std::accumulate(dbl_col.begin(), dbl_col.end(), 0.0);

	ASSERT_EQ(4, dbl_col.length());
	ASSERT_EQ(10.0, sum);
}


TEST(TestDatastore, CreateDataTable)
{
	using namespace hgps::data;

	auto flt_values = std::vector<float>{ 5.7f, 7.13f, 15.37f, 0.0f, 20.75f };
	auto int_values = std::vector<int>{ 15, 78, 154, 0, 200 };

	auto ftl_duilder = FloatDataTableColumnBuilder{ "Floats" };
	auto dbl_duilder = DoubleDataTableColumnBuilder{ "Doubles" };
	auto int_duilder = IntegerDataTableColumnBuilder{ "Integer" };

	for (size_t i = 0; i < flt_values.size(); i++)
	{
		ftl_duilder.append(flt_values[i]);
		dbl_duilder.append(flt_values[i] + 1.0);

		int_duilder.append(int_values[i]);
	}

	auto table = DataTable();
	table.add(ftl_duilder.build());
	table.add(dbl_duilder.build());
	table.add(int_duilder.build());

	ASSERT_EQ(3, table.count());
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