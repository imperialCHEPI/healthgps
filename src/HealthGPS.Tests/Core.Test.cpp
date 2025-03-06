#include "HealthGPS.Core/api.h"
#include "HealthGPS.Core/string_util.h"
#include "pch.h"

#include <numeric>
#include <sstream>

#ifdef _MSC_VER
#pragma warning(disable : 26439) // This kind of function should not throw. Declare it 'noexcept'
#pragma warning(disable : 26495) // Variable is uninitialized
#pragma warning(disable : 26819) // Unannotated fallthrough between switch labels
#pragma warning(disable : 26498) // The function is constexpr, mark variable constexpr if
                                 // compile-time evaluation is desired
#pragma warning(                                                                                   \
    disable : 6285) // (<non-zero constant> || <non-zero constant>) is always a non-zero constant
#endif

TEST(TestCore, CreateCountry) {
    using namespace hgps::core;

    unsigned short id = 826;
    const auto *uk = "United Kingdom";

    auto c = Country{.code = id, .name = uk, .alpha2 = "GB", .alpha3 = "GBR"};

    EXPECT_EQ(id, c.code);
    EXPECT_EQ(uk, c.name);
    EXPECT_EQ("GB", c.alpha2);
    EXPECT_EQ("GBR", c.alpha3);
}

TEST(TestCore, CreateTableColumnWithNulls) {
    using namespace hgps::core;

    auto str_col = StringDataTableColumn{"string", {"Cat", "Dog", ""}, {true, true, false}};
    auto flt_col =
        FloatDataTableColumn("float", {5.7f, 15.37f, 0.0f, 20.75f}, {true, true, false, true});
    auto dbl_col = DoubleDataTableColumn("double", {7.13, 15.37, 20.75}, {true, true, true});
    auto int_col = IntegerDataTableColumn("integer", {0, 15, 200}, {false, true, true});

    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    ASSERT_EQ(3, str_col.size());
    ASSERT_EQ(1, str_col.null_count());
    ASSERT_EQ("Dog", *str_col.value_safe(1));
    ASSERT_EQ("Dog", str_col.value_unsafe(1));
    ASSERT_TRUE(str_col.is_null(2));
    ASSERT_FALSE(str_col.is_valid(2));
    ASSERT_FALSE(str_col.is_null(0));
    ASSERT_TRUE(str_col.is_valid(0));

    ASSERT_EQ(4, flt_col.size());
    ASSERT_EQ(1, flt_col.null_count());
    ASSERT_EQ(15.37f, *flt_col.value_safe(1));
    ASSERT_EQ(15.37f, flt_col.value_unsafe(1));
    ASSERT_TRUE(flt_col.is_null(2));
    ASSERT_FALSE(flt_col.is_valid(2));
    ASSERT_FALSE(flt_col.is_null(0));
    ASSERT_TRUE(flt_col.is_valid(0));

    ASSERT_EQ(3, dbl_col.size());
    ASSERT_EQ(0, dbl_col.null_count());
    ASSERT_EQ(15.37, *dbl_col.value_safe(1));
    ASSERT_EQ(15.37, dbl_col.value_unsafe(1));
    ASSERT_FALSE(dbl_col.is_null(1));
    ASSERT_TRUE(dbl_col.is_valid(1));

    ASSERT_EQ(3, int_col.size());
    ASSERT_EQ(1, int_col.null_count());
    ASSERT_EQ(15, *int_col.value_safe(1));
    ASSERT_EQ(15, int_col.value_unsafe(1));
    ASSERT_TRUE(int_col.is_null(0));
    ASSERT_FALSE(int_col.is_valid(0));
    ASSERT_FALSE(int_col.is_null(1));
    ASSERT_TRUE(int_col.is_valid(1));
    // NOLINTEND(bugprone-unchecked-optional-access)
}

TEST(TestCore, CreateTableColumnWithoutNulls) {
    using namespace hgps::core;

    auto str_col = StringDataTableColumn("string", {"Cat", "Dog", "Cow"});
    auto flt_col = FloatDataTableColumn("float", {7.13f, 15.37f, 0.0f, 20.75f});
    auto dbl_col = DoubleDataTableColumn("double", {7.13, 15.37, 20.75});
    auto int_col = IntegerDataTableColumn("integer", {0, 15, 200});

    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    ASSERT_EQ(3, str_col.size());
    ASSERT_EQ(0, str_col.null_count());
    ASSERT_EQ("Dog", *str_col.value_safe(1));
    ASSERT_EQ("Dog", str_col.value_unsafe(1));
    ASSERT_TRUE(str_col.is_valid(0));
    ASSERT_FALSE(str_col.is_null(0));

    ASSERT_EQ(4, flt_col.size());
    ASSERT_EQ(0, flt_col.null_count());
    ASSERT_EQ(15.37f, *flt_col.value_safe(1));
    ASSERT_EQ(15.37f, flt_col.value_unsafe(1));
    ASSERT_TRUE(flt_col.is_valid(0));
    ASSERT_FALSE(flt_col.is_null(0));

    ASSERT_EQ(3, dbl_col.size());
    ASSERT_EQ(0, dbl_col.null_count());
    ASSERT_EQ(15.37, *dbl_col.value_safe(1));
    ASSERT_EQ(15.37, dbl_col.value_unsafe(1));
    ASSERT_TRUE(dbl_col.is_valid(1));
    ASSERT_FALSE(dbl_col.is_null(1));

    ASSERT_EQ(3, int_col.size());
    ASSERT_EQ(0, int_col.null_count());
    ASSERT_EQ(15, *int_col.value_safe(1));
    ASSERT_EQ(15, int_col.value_unsafe(1));
    ASSERT_TRUE(int_col.is_valid(0));
    ASSERT_FALSE(int_col.is_null(0));
    // NOLINTEND(bugprone-unchecked-optional-access)
}

TEST(TestCore, CreateTableColumnFailWithLenMismatch) {
    using namespace hgps::core;

    ASSERT_THROW(IntegerDataTableColumn("integer", {5, 15, 0, 20}, {true, false, true}),
                 std::out_of_range);
}

TEST(TestCore, CreateTableColumnFailWithShortName) {
    using namespace hgps::core;

    ASSERT_THROW(IntegerDataTableColumn("f", {15, 0, 20}, {true, false, true}),
                 std::invalid_argument);
}

TEST(TestCore, CreateTableColumnFailWithInvalidName) {
    try {
        // This should throw an exception since the name contains invalid characters
        hgps::core::IntegerDataTableColumn col("invalid name with spaces", {1, 2, 3});
        FAIL() << "Expected an exception for invalid column name, but none was thrown";
    } catch (const std::invalid_argument &e) {
        // Expected exception
        ASSERT_STREQ("Column name cannot contain spaces or special characters", e.what());
    } catch (...) {
        FAIL() << "Expected std::invalid_argument exception, but a different exception was thrown";
    }
}

TEST(TestCore, TableColumnIterator) {
    using namespace hgps::core;

    auto dbl_col = DoubleDataTableColumn("double", {1.5, 3.5, 2.0, 0.0, 3.0, 0.0, 5.0},
                                         {true, true, true, false, true, false, true});

    ASSERT_TRUE(dbl_col.size() > 0);               // Ensure the column is not empty
    ASSERT_TRUE(dbl_col.begin() != dbl_col.end()); // Ensure we can iterate

    double loop_sum = 0.0;
    size_t count = 0;
    for (const auto v : dbl_col) {
        loop_sum += v;
        count++;
    }
    ASSERT_EQ(count, dbl_col.size()); // Ensure we iterated through all elements

    auto sum = std::accumulate(dbl_col.begin(), dbl_col.end(), 0.0);

    ASSERT_EQ(7, dbl_col.size());
    ASSERT_EQ(2, dbl_col.null_count());
    ASSERT_EQ(15.0, sum);
    ASSERT_EQ(loop_sum, sum);
}

TEST(TestCore, CreateDataTable) {
    using namespace hgps::core;

    auto str_values = std::vector<std::string>{"Cat", "Dog", "", "Cow", "Fox"};
    auto flt_values = std::vector<float>{5.7f, 7.13f, 15.37f, 0.0f, 20.75f};
    auto int_values = std::vector<int>{15, 78, 154, 0, 200};

    auto str_builder = StringDataTableColumnBuilder{"String"};
    auto ftl_builder = FloatDataTableColumnBuilder{"Floats"};
    auto dbl_builder = DoubleDataTableColumnBuilder{"Doubles"};
    auto int_builder = IntegerDataTableColumnBuilder{"Integer"};

    ASSERT_EQ(str_values.size(), flt_values.size());
    ASSERT_EQ(flt_values.size(), int_values.size());

    for (size_t i = 0; i < flt_values.size(); i++) {
        str_values[i].empty() ? str_builder.append_null() : str_builder.append(str_values[i]);
        flt_values[i] == float{} ? ftl_builder.append_null() : ftl_builder.append(flt_values[i]);
        flt_values[i] == double{} ? dbl_builder.append_null()
                                  : dbl_builder.append(flt_values[i] + 1.0);
        int_values[i] == int{} ? int_builder.append_null() : int_builder.append(int_values[i]);
    }

    auto table = DataTable();
    table.add(str_builder.build());
    table.add(ftl_builder.build());
    table.add(dbl_builder.build());
    table.add(int_builder.build());

    // Casting to columns type
    const auto &col = table.column("Integer");
    ASSERT_TRUE(&col != nullptr);
    const auto *int_col_ptr = dynamic_cast<const IntegerDataTableColumn *>(&col);
    ASSERT_TRUE(int_col_ptr != nullptr);
    const auto &int_col = *int_col_ptr;

    ASSERT_TRUE(col.size() > 1); // Ensure we have at least 2 elements before accessing index 1
    auto slow_value = std::any_cast<int>(col.value(1));
    auto safe_value = int_col.value_safe(1);
    ASSERT_TRUE(safe_value.has_value()); // Check if the value exists before using it
    auto fast_value = int_col.value_unsafe(1);

    ASSERT_EQ(4, table.num_columns());
    ASSERT_EQ(5, table.num_rows());
    ASSERT_EQ(table.num_rows(), int_col.size());
    ASSERT_EQ(78, slow_value);
    ASSERT_EQ(slow_value, *safe_value);
    ASSERT_EQ(slow_value, fast_value);
}

TEST(TestCore, DataTableFailWithColumnLenMismath) {
    using namespace hgps::core;

    auto flt_values = std::vector<float>{7.13f, 15.37f, 0.0f, 20.75f};
    auto int_values = std::vector<int>{154, 0, 200};

    auto table = DataTable();
    table.add(std::make_unique<FloatDataTableColumn>("float", flt_values,
                                                     std::vector<bool>{true, true, false, true}));

    ASSERT_THROW(table.add(std::make_unique<IntegerDataTableColumn>(
                     "integer", int_values, std::vector<bool>{true, false, true})),
                 std::invalid_argument);
}

TEST(TestCore, DataTableFailDuplicateColumn) {
    using namespace hgps::core;

    auto flt_values = std::vector<float>{7.13f, 15.37f, 0.0f, 20.75f};
    auto int_values = std::vector<int>{100, 154, 0, 200};

    auto table = DataTable();
    table.add(std::make_unique<FloatDataTableColumn>("Number", flt_values,
                                                     std::vector<bool>{true, true, false, true}));

    ASSERT_THROW(table.add(std::make_unique<IntegerDataTableColumn>(
                     "number", int_values, std::vector<bool>{true, true, false, true})),
                 std::invalid_argument);
}

TEST(TestCore, CaseInsensitiveString) {
    using namespace hgps::core;

    const auto *source = "The quick brown fox jumps over the lazy dog";

    ASSERT_TRUE(case_insensitive::equals("fox", "Fox"));
    ASSERT_TRUE(case_insensitive::contains(source, "FOX"));
    ASSERT_TRUE(case_insensitive::starts_with(source, "the"));
    ASSERT_TRUE(case_insensitive::ends_with(source, "Dog"));

    ASSERT_EQ(std::weak_ordering::less, case_insensitive::compare("Dog", "Fox"));
    ASSERT_EQ(std::weak_ordering::equivalent, case_insensitive::compare("fox", "Fox"));
    ASSERT_EQ(std::weak_ordering::greater, case_insensitive::compare("Dog", "Cat"));
}

TEST(TestCore, CaseInsensitiveVectorIndexOf) {
    using namespace hgps::core;

    std::vector<std::string> source{"The",  "quick", "brown", "fox", "jumps",
                                    "over", "a",     "lazy",  "dog"};

    for (size_t i = 0; i < source.size(); i++) {
        ASSERT_EQ(i, case_insensitive::index_of(source, source[i]));
    }

    for (size_t i = 0; i < source.size(); i++) {
        ASSERT_EQ(i, case_insensitive::index_of(source, to_upper(source[i])));
    }

    EXPECT_EQ(0, case_insensitive::index_of(source, "the"));
    EXPECT_EQ(2, case_insensitive::index_of(source, "brown"));
    EXPECT_EQ(2, case_insensitive::index_of(source, "BROWN"));
    EXPECT_EQ(8, case_insensitive::index_of(source, "dog"));
    EXPECT_EQ(8, case_insensitive::index_of(source, "DoG"));
    EXPECT_EQ(-1, case_insensitive::index_of(source, "Cat"));
}

TEST(TestCore, CaseInsensitiveVectorContains) {
    using namespace hgps::core;

    std::vector<std::string> source{"The",  "quick", "brown", "fox", "jumps",
                                    "over", "a",     "lazy",  "dog"};

    for (size_t i = 0; i < source.size(); i++) {
        ASSERT_TRUE(case_insensitive::contains(source, source[i]));
    }

    for (size_t i = 0; i < source.size(); i++) {
        ASSERT_TRUE(case_insensitive::contains(source, to_upper(source[i])));
    }

    EXPECT_TRUE(case_insensitive::contains(source, "the"));
    EXPECT_TRUE(case_insensitive::contains(source, "brown"));
    EXPECT_TRUE(case_insensitive::contains(source, "BROWN"));
    EXPECT_TRUE(case_insensitive::contains(source, "dog"));
    EXPECT_TRUE(case_insensitive::contains(source, "DoG"));
    EXPECT_FALSE(case_insensitive::contains(source, "Cat"));
}

TEST(TestCore, CaseInsensitiveMap) {
    using namespace hgps::core;

    std::map<std::string, int, case_insensitive::comparator> data{
        {"cat", 1},
        {"Dog", 2},
        {"ODD", 3},
    };

    EXPECT_TRUE(data.contains("CAT"));
    EXPECT_TRUE(data.contains("dog"));
    EXPECT_TRUE(data.contains("odd"));
    EXPECT_FALSE(data.contains("Rat"));
}

TEST(TestCore, SplitDelimitedString) {
    using namespace hgps::core;

    const auto *source = "The quick brown fox jumps over the lazy dog";
    auto parts = split_string(source, " ");

    const auto *csv_source = "The,quick,brown,fox,jumps,over,the,lazy,dog";
    auto csv_parts = split_string(csv_source, ",");

    ASSERT_EQ(9, parts.size());
    ASSERT_EQ("The", parts.front());
    ASSERT_EQ("dog", parts.back());

    ASSERT_EQ(parts.size(), csv_parts.size());
    ASSERT_EQ(parts.front(), csv_parts.front());
    ASSERT_EQ(parts.back(), csv_parts.back());
}

// Tests for column_numeric.h and column_primitive.h - Mahima
// These tests verify the basic operations of numeric data table columns
TEST(TestCore, FloatDataTableColumnOperations) {
    using namespace hgps::core;

    FloatDataTableColumn column("test_float", std::vector<float>{}, std::vector<bool>{});
    ASSERT_EQ("float", column.type());
}

TEST(TestCore, DoubleDataTableColumnOperations) {
    using namespace hgps::core;

    DoubleDataTableColumn column("test_double", std::vector<double>{}, std::vector<bool>{});
    ASSERT_EQ("double", column.type());
}

TEST(TestCore, IntegerDataTableColumnOperations) {
    using namespace hgps::core;

    IntegerDataTableColumn column("test_int", std::vector<int>{}, std::vector<bool>{});
    ASSERT_EQ("integer", column.type());
}

// Tests for column_primitive.h - Mahima
// Verifies primitive data table column operations including iterators and value access
TEST(TestCore, PrimitiveDataTableColumnOperations) {
    using namespace hgps::core;

    std::vector<int> data{42};
    std::vector<bool> validity{true};
    IntegerDataTableColumn column("test_primitive", data, validity);

    // Test basic operations
    ASSERT_EQ(1, column.size());
    ASSERT_EQ(42, column.value_unsafe(0));
    ASSERT_TRUE(column.is_valid(0));
    ASSERT_FALSE(column.is_null(0));

    // Test value_safe
    auto value = column.value_safe(0);
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(42, *value);

    // Test iterator operations
    const std::vector<int> iter_data{1, 2, 3};
    const std::vector<bool> iter_validity{true, true, true};
    IntegerDataTableColumn iter_column("test_iter", iter_data, iter_validity);

    auto it = iter_column.begin();
    ASSERT_EQ(1, *it);
    ++it;
    ASSERT_EQ(2, *it);
    ++it;
    ASSERT_EQ(3, *it);
    ++it;
    ASSERT_EQ(it, iter_column.end());
}

// Tests for datatable.cpp - Mahima
// Comprehensive tests for DataTable class operations including column management
TEST(TestCore, DataTableComprehensiveOperations) {
    using namespace hgps::core;

    DataTable table;

    // Test construction and basic operations
    ASSERT_EQ(0, table.num_rows());
    ASSERT_EQ(0, table.num_columns());

    // Add columns
    table.add(
        std::make_unique<IntegerDataTableColumn>("id", std::vector<int>{}, std::vector<bool>{}));
    table.add(std::make_unique<DoubleDataTableColumn>("value", std::vector<double>{},
                                                      std::vector<bool>{}));

    ASSERT_EQ(0, table.num_rows());
    ASSERT_EQ(2, table.num_columns());

    // Test column access
    const auto &id_col = table.column("id");
    const auto &value_col = table.column("value");

    ASSERT_EQ("id", id_col.name());
    ASSERT_EQ("value", value_col.name());

    // Test column existence
    ASSERT_NO_THROW(table.column("id"));
    ASSERT_THROW(table.column("nonexistent"), std::out_of_range);

    // Test duplicate column
    ASSERT_THROW(table.add(std::make_unique<IntegerDataTableColumn>("id", std::vector<int>{},
                                                                    std::vector<bool>{})),
                 std::invalid_argument);
}
