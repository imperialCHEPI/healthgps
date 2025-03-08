#include "HealthGPS.Core/api.h"
#include "HealthGPS.Core/datatable.h"
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

    // Create a simple column with a mix of nulls and values
    auto str_col = StringDataTableColumn{"string", {"Cat", "Dog", "Mouse"}, {true, true, false}};
    
    // Check basic properties
    ASSERT_EQ(3, str_col.size());
    ASSERT_EQ(1, str_col.null_count());
    
    // Check that we can identify which row is null
    ASSERT_TRUE(str_col.is_valid(0));
    ASSERT_TRUE(str_col.is_valid(1));
    ASSERT_FALSE(str_col.is_valid(2));
    
    // Check that value_safe works
    auto val1 = str_col.value_safe(1);
    ASSERT_TRUE(val1.has_value());
    ASSERT_EQ("Dog", val1.value());
    
    // Check null position
    auto val2 = str_col.value_safe(2);
    ASSERT_FALSE(val2.has_value());
}

TEST(TestCore, CreateTableColumnWithoutNulls) {
    using namespace hgps::core;

    // Create a simple column with no nulls
    auto str_col = StringDataTableColumn("string", {"Cat", "Dog", "Cow"});
    
    // Check basic properties
    ASSERT_EQ(3, str_col.size());
    ASSERT_EQ(0, str_col.null_count());
    
    // Check that all values are valid
    ASSERT_TRUE(str_col.is_valid(0));
    ASSERT_TRUE(str_col.is_valid(1));
    ASSERT_TRUE(str_col.is_valid(2));
    
    // Check that value_safe works
    auto val = str_col.value_safe(1);
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ("Dog", val.value());
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
    using namespace hgps::core;

    DataTable table;

    // Test with empty column name
    EXPECT_THROW(table.add(std::make_unique<IntegerDataTableColumn>("", std::vector<int>{},
                                                                    std::vector<bool>{})),
                 std::invalid_argument);

    // Test with whitespace-only column name
    EXPECT_THROW(table.add(std::make_unique<IntegerDataTableColumn>("   ", std::vector<int>{},
                                                                    std::vector<bool>{})),
                 std::invalid_argument);

    // Test with column name containing invalid characters
    EXPECT_THROW(table.add(std::make_unique<IntegerDataTableColumn>(
                     "column name", std::vector<int>{}, std::vector<bool>{})),
                 std::invalid_argument);
    EXPECT_THROW(table.add(std::make_unique<IntegerDataTableColumn>(
                     "column-name", std::vector<int>{}, std::vector<bool>{})),
                 std::invalid_argument);
    EXPECT_THROW(table.add(std::make_unique<IntegerDataTableColumn>(
                     "column@name", std::vector<int>{}, std::vector<bool>{})),
                 std::invalid_argument);

    // Test with duplicate column name
    table.add(std::make_unique<IntegerDataTableColumn>("valid_column", std::vector<int>{},
                                                       std::vector<bool>{}));
    EXPECT_THROW(table.add(std::make_unique<IntegerDataTableColumn>(
                     "valid_column", std::vector<int>{}, std::vector<bool>{})),
                 std::invalid_argument);
}

TEST(TestCore, TableColumnIterator) {
    using namespace hgps::core;

    // Create a simple column with no nulls for simplicity
    auto dbl_col = DoubleDataTableColumn("double", {1.5, 3.5, 2.0, 5.0});

    // Basic iteration checks
    ASSERT_EQ(4, dbl_col.size());
    ASSERT_EQ(0, dbl_col.null_count());
    
    // Verify iterator basics
    ASSERT_TRUE(dbl_col.begin() != dbl_col.end());
    
    // Calculate sum using for loop
    double sum = 0.0;
    for (const auto& val : dbl_col) {
        sum += val;
    }
    
    // Verify expected sum
    ASSERT_DOUBLE_EQ(12.0, sum);
}

TEST(TestCore, CreateDataTable) {
    using namespace hgps::core;

    // Create a simple table with one column
    auto table = DataTable();
    
    // Create a simple integer column
    std::vector<int> values{10, 20, 30};
    auto int_col = std::make_unique<IntegerDataTableColumn>("numbers", values);
    
    // Add column to table
    table.add(std::move(int_col));
    
    // Basic checks
    ASSERT_EQ(1, table.num_columns());
    ASSERT_EQ(3, table.num_rows());
    
    // Check column retrieval and content
    const auto& col = table.column("numbers");
    ASSERT_EQ("numbers", col.name());
    
    // Test value access using any_cast with error handling
    try {
        int val = std::any_cast<int>(col.value(1));
        ASSERT_EQ(20, val);
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Bad any_cast: " << e.what() << ", type: " << col.value(1).type().name();
    }
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

    // Create a simple column with no nulls
    std::vector<int> data = {42, 7, 13};
    IntegerDataTableColumn column("test", data);
    
    // Test basic properties
    ASSERT_EQ(3, column.size());
    ASSERT_EQ(0, column.null_count());
    
    // Test value access using value_safe
    auto val0 = column.value_safe(0);
    ASSERT_TRUE(val0.has_value());
    ASSERT_EQ(42, val0.value());
    
    // Test any casting with error handling
    try {
        auto any_value = column.value(0);
        int casted_value = std::any_cast<int>(any_value);
        ASSERT_EQ(42, casted_value);
    } catch (const std::bad_any_cast& e) {
        FAIL() << "Bad any_cast: " << e.what();
    }
    
    // Test iteration
    int sum = 0;
    for (const auto& val : column) {
        sum += val;
    }
    ASSERT_EQ(42 + 7 + 13, sum);
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

TEST(TestCore, ColumnNameValidation) {
    using namespace hgps::core;

    EXPECT_TRUE(is_valid_column_name("valid_name"));
    EXPECT_TRUE(is_valid_column_name("validName"));
    EXPECT_TRUE(is_valid_column_name("valid_name_123"));

    EXPECT_FALSE(is_valid_column_name(""));
    EXPECT_FALSE(is_valid_column_name("   "));
    EXPECT_FALSE(is_valid_column_name("invalid name"));
    EXPECT_FALSE(is_valid_column_name("invalid-name"));
    EXPECT_FALSE(is_valid_column_name("invalid@name"));
}
