#include "HealthGPS.Core/api.h"
#include "HealthGPS.Core/string_util.h"
#include "pch.h"

#include <numeric>
#include <sstream>

TEST(TestCore, CurrentApiVersion) {
    using namespace hgps::core;

    std::stringstream ss;
    ss << API_MAJOR << "." << API_MINOR << "." << API_PATCH;

    EXPECT_EQ(API_MAJOR, Version::GetMajor());
    EXPECT_EQ(API_MINOR, Version::GetMinor());
    EXPECT_EQ(API_PATCH, Version::GetPatch());
    EXPECT_EQ(ss.str(), Version::GetVersion());

    EXPECT_TRUE(Version::HasFeature("COUNTRY"));
    EXPECT_FALSE(Version::HasFeature("DISEASES"));

    EXPECT_TRUE(Version::IsAtLeast(API_MAJOR, API_MINOR, API_PATCH));
    EXPECT_FALSE(Version::IsAtLeast(API_MAJOR + 1, 0, 0));
    EXPECT_FALSE(Version::IsAtLeast(API_MAJOR, API_MINOR + 1, API_PATCH));
    EXPECT_FALSE(Version::IsAtLeast(API_MAJOR, API_MINOR, API_PATCH + 1));
}

TEST(TestCore, CreateCountry) {
    using namespace hgps::core;

    unsigned short id = 826;
    auto uk = "United Kingdom";

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
}

TEST(TestCore, CreateTableColumnWithoutNulls) {
    using namespace hgps::core;

    auto str_col = StringDataTableColumn("string", {"Cat", "Dog", "Cow"});
    auto flt_col = FloatDataTableColumn("float", {7.13f, 15.37f, 0.0f, 20.75f});
    auto dbl_col = DoubleDataTableColumn("double", {7.13, 15.37, 20.75});
    auto int_col = IntegerDataTableColumn("integer", {0, 15, 200});

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

    ASSERT_THROW(IntegerDataTableColumn("5nteger", {15, 0, 20}, {true, false, true}),
                 std::invalid_argument);
}

TEST(TestCore, TableColumnIterator) {
    using namespace hgps::core;

    auto dbl_col = DoubleDataTableColumn("double", {1.5, 3.5, 2.0, 0.0, 3.0, 0.0, 5.0},
                                         {true, true, true, false, true, false, true});

    double loop_sum = 0.0;
    for (auto &item : dbl_col) {
        loop_sum += item.value_or(0.0);
    }

    auto sum = std::accumulate(dbl_col.begin(), dbl_col.end(), 0.0,
                               [](auto a, auto b) { return b.has_value() ? a + b.value() : a; });

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
    auto &col = table.column("Integer");
    auto &int_col = static_cast<const IntegerDataTableColumn &>(col);

    auto slow_value = std::any_cast<int>(col.value(1));
    auto safe_value = int_col.value_safe(1);
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
    table.add(std::make_unique<FloatDataTableColumn>("float", std::move(flt_values),
                                                     std::vector<bool>{true, true, false, true}));

    ASSERT_THROW(table.add(std::make_unique<IntegerDataTableColumn>(
                     "integer", std::move(int_values), std::vector<bool>{true, false, true})),
                 std::invalid_argument);
}

TEST(TestCore, DataTableFailDuplicateColumn) {
    using namespace hgps::core;

    auto flt_values = std::vector<float>{7.13f, 15.37f, 0.0f, 20.75f};
    auto int_values = std::vector<int>{100, 154, 0, 200};

    auto table = DataTable();
    table.add(std::make_unique<FloatDataTableColumn>("Number", std::move(flt_values),
                                                     std::vector<bool>{true, true, false, true}));

    ASSERT_THROW(table.add(std::make_unique<IntegerDataTableColumn>(
                     "number", std::move(int_values), std::vector<bool>{true, true, false, true})),
                 std::invalid_argument);
}

TEST(TestCore, CaseInsensitiveString) {
    using namespace hgps::core;

    auto source = "The quick brown fox jumps over the lazy dog";

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

    auto source = "The quick brown fox jumps over the lazy dog";
    auto parts = split_string(source, " ");

    auto csv_source = "The,quick,brown,fox,jumps,over,the,lazy,dog";
    auto csv_parts = split_string(csv_source, ",");

    ASSERT_EQ(9, parts.size());
    ASSERT_EQ("The", parts.front());
    ASSERT_EQ("dog", parts.back());

    ASSERT_EQ(parts.size(), csv_parts.size());
    ASSERT_EQ(parts.front(), csv_parts.front());
    ASSERT_EQ(parts.back(), csv_parts.back());
}
