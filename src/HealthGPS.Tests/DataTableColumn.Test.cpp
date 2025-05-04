#include "HealthGPS.Core/api.h"
#include "HealthGPS.Core/column_numeric.h"
#include "HealthGPS.Core/column_primitive.h"
#include "HealthGPS.Core/datatable.h"
#include "pch.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Tests for column_numeric.h and column_primitive.h
// Created- Mahima
// These tests comprehensively verify all column operations

using namespace hgps::core;

// Test StringDataTableColumn class - lines 16, 20-21
TEST(DataTableColumnTest, StringDataTableColumnOperations) {
    // Test construction and type
    StringDataTableColumn column("test_string", std::vector<std::string>{"A", "B", "C"});
    ASSERT_EQ("string", column.type());
    ASSERT_EQ(3, column.size());
    ASSERT_EQ("test_string", column.name());

    // Test clone operation (tests clone_impl)
    auto clone_ptr = column.clone();
    ASSERT_NE(nullptr, clone_ptr);
    ASSERT_EQ("string", clone_ptr->type());
    ASSERT_EQ(3, clone_ptr->size());
    ASSERT_EQ("test_string", clone_ptr->name());

    // Test value access
    ASSERT_EQ("A", std::any_cast<std::string>(column.value(0)));
    ASSERT_EQ("B", std::any_cast<std::string>(column.value(1)));
    ASSERT_EQ("C", std::any_cast<std::string>(column.value(2)));

    // Test out of bounds - is_null should return true for out of bounds
    ASSERT_TRUE(column.is_null(100));
    // Test out of bounds - is_valid should return false for out of bounds
    ASSERT_FALSE(column.is_valid(100));
}

// Test column name validation in IntegerDataTableColumn - lines 61, 64
TEST(DataTableColumnTest, IntegerDataTableColumnNameValidation) {
    // Test empty name
    ASSERT_THROW(IntegerDataTableColumn("", std::vector<int>{1, 2, 3}), std::invalid_argument);

    // Test whitespace-only name
    ASSERT_THROW(IntegerDataTableColumn("   ", std::vector<int>{1, 2, 3}), std::invalid_argument);

    // Test invalid characters in name
    ASSERT_THROW(IntegerDataTableColumn("invalid name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);
    ASSERT_THROW(IntegerDataTableColumn("invalid-name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);
    ASSERT_THROW(IntegerDataTableColumn("invalid@name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);
    ASSERT_THROW(IntegerDataTableColumn("invalid#name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);
    ASSERT_THROW(IntegerDataTableColumn("invalid$name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);
    ASSERT_THROW(IntegerDataTableColumn("invalid%name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);
    ASSERT_THROW(IntegerDataTableColumn("invalid^name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);
    ASSERT_THROW(IntegerDataTableColumn("invalid&name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);
    ASSERT_THROW(IntegerDataTableColumn("invalid*name", std::vector<int>{1, 2, 3}),
                 std::invalid_argument);

    // Test valid names pass
    ASSERT_NO_THROW(IntegerDataTableColumn("valid_name", std::vector<int>{1, 2, 3}));
    ASSERT_NO_THROW(IntegerDataTableColumn("validName", std::vector<int>{1, 2, 3}));
    ASSERT_NO_THROW(IntegerDataTableColumn("valid_name_123", std::vector<int>{1, 2, 3}));
}

// Test PrimitiveDataTableColumn methods - lines 84, 91
TEST(DataTableColumnTest, PrimitiveColumnNullAndValidChecks) {
    // Create column with both valid and null values
    std::vector<int> data{1, 2, 3, 4, 5};
    std::vector<bool> validity{false, true, false, true, false}; // true means null
    IntegerDataTableColumn column("test_int", data, validity);

    // Test is_null for different indexes
    ASSERT_FALSE(column.is_null(0));  // Valid
    ASSERT_TRUE(column.is_null(1));   // Null
    ASSERT_FALSE(column.is_null(2));  // Valid
    ASSERT_TRUE(column.is_null(3));   // Null
    ASSERT_FALSE(column.is_null(4));  // Valid
    ASSERT_TRUE(column.is_null(100)); // Out of bounds

    // Test is_valid for different indexes
    ASSERT_TRUE(column.is_valid(0));    // Valid
    ASSERT_FALSE(column.is_valid(1));   // Null
    ASSERT_TRUE(column.is_valid(2));    // Valid
    ASSERT_FALSE(column.is_valid(3));   // Null
    ASSERT_TRUE(column.is_valid(4));    // Valid
    ASSERT_FALSE(column.is_valid(100)); // Out of bounds
}

// Test double column - for better code coverage
TEST(DataTableColumnTest, DoubleDataTableColumnCompleteTest) {
    // Create a DoubleDataTableColumn
    std::vector<double> data{1.1, 2.2, 3.3};
    DoubleDataTableColumn column("test_double", data);

    // Test type and size
    ASSERT_EQ("double", column.type());
    ASSERT_EQ(3, column.size());

    // Test clone operation
    auto clone_ptr = column.clone();
    ASSERT_NE(nullptr, clone_ptr);
    ASSERT_EQ("double", clone_ptr->type());
    ASSERT_EQ(3, clone_ptr->size());

    // Test value access
    ASSERT_DOUBLE_EQ(1.1, std::any_cast<double>(column.value(0)));
    ASSERT_DOUBLE_EQ(2.2, std::any_cast<double>(column.value(1)));
    ASSERT_DOUBLE_EQ(3.3, std::any_cast<double>(column.value(2)));

    // Test null checking
    ASSERT_FALSE(column.is_null(0));
    ASSERT_TRUE(column.is_valid(0));

    // Test out of bounds
    ASSERT_TRUE(column.is_null(100));
    ASSERT_FALSE(column.is_valid(100));
}

// Test visitor pattern implementation - lines 126, 128, 130, 132, 134
class TestVisitor : public DataTableColumnVisitor {
  public:
    std::string visited_type = "none";

    void visit(const StringDataTableColumn & /* column */) override { visited_type = "string"; }

    void visit(const IntegerDataTableColumn & /* column */) override { visited_type = "integer"; }

    void visit(const FloatDataTableColumn & /* column */) override { visited_type = "float"; }

    void visit(const DoubleDataTableColumn & /* column */) override { visited_type = "double"; }
};

TEST(DataTableColumnTest, VisitorPattern) {
    // Test visitor with StringDataTableColumn
    {
        StringDataTableColumn column("test_string", std::vector<std::string>{"A"});
        TestVisitor visitor;
        column.accept(visitor);
        ASSERT_EQ("string", visitor.visited_type);
    }

    // Test visitor with IntegerDataTableColumn
    {
        IntegerDataTableColumn column("test_int", std::vector<int>{1});
        TestVisitor visitor;
        column.accept(visitor);
        ASSERT_EQ("integer", visitor.visited_type);
    }

    // Test visitor with FloatDataTableColumn
    {
        FloatDataTableColumn column("test_float", std::vector<float>{1.0f});
        TestVisitor visitor;
        column.accept(visitor);
        ASSERT_EQ("float", visitor.visited_type);
    }

    // Test visitor with DoubleDataTableColumn
    {
        DoubleDataTableColumn column("test_double", std::vector<double>{1.0});
        TestVisitor visitor;
        column.accept(visitor);
        ASSERT_EQ("double", visitor.visited_type);
    }
}

// Test iterators - line 118, 122, 141
TEST(DataTableColumnTest, ColumnIterators) {
    // Create column with test data
    std::vector<int> data{10, 20, 30, 40, 50};
    IntegerDataTableColumn column("test_int", data);

    // Test begin and end iterators
    auto it = column.begin();
    auto end = column.end();

    // Verify iterator operations
    ASSERT_NE(it, end);
    ASSERT_EQ(10, *it);

    // Iterate through data and verify values using range-based for loop
    std::vector<int> collected_values;
    for (const auto &value : column) {
        collected_values.push_back(value);
    }

    ASSERT_EQ(data.size(), collected_values.size());
    for (size_t i = 0; i < data.size(); ++i) {
        ASSERT_EQ(data[i], collected_values[i]);
    }

    // Test iterator increment
    auto it2 = column.begin();
    ASSERT_EQ(10, *it2);
    ++it2;
    ASSERT_EQ(20, *it2);
    ++it2;
    ASSERT_EQ(30, *it2);

    // Test has_value method
    ASSERT_TRUE(column.has_value(0));
    ASSERT_TRUE(column.has_value(4));
    ASSERT_FALSE(column.has_value(5)); // Out of bounds
}

// Test float column operations for complete coverage
TEST(DataTableColumnTest, FloatDataTableColumnCompleteTest) {
    // Create a FloatDataTableColumn
    std::vector<float> data{1.1f, 2.2f, 3.3f};
    FloatDataTableColumn column("test_float", data);

    // Test type and size
    ASSERT_EQ("float", column.type());
    ASSERT_EQ(3, column.size());

    // Test clone operation
    auto clone_ptr = column.clone();
    ASSERT_NE(nullptr, clone_ptr);
    ASSERT_EQ("float", clone_ptr->type());
    ASSERT_EQ(3, clone_ptr->size());

    // Test value access
    ASSERT_FLOAT_EQ(1.1f, std::any_cast<float>(column.value(0)));
    ASSERT_FLOAT_EQ(2.2f, std::any_cast<float>(column.value(1)));
    ASSERT_FLOAT_EQ(3.3f, std::any_cast<float>(column.value(2)));

    // Test null checking
    ASSERT_FALSE(column.is_null(0));
    ASSERT_TRUE(column.is_valid(0));

    // Test out of bounds
    ASSERT_TRUE(column.is_null(100));
    ASSERT_FALSE(column.is_valid(100));
}
