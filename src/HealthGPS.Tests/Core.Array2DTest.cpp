#include "HealthGPS.Core/array2d.h"
#include "pch.h"

#include <optional>

TEST(TestCore_Array2D, CreateEmptyStorage) {
    using namespace hgps::core;

    auto d3x2 = DoubleArray2D(3, 2);

    ASSERT_EQ(3, d3x2.rows());
    ASSERT_EQ(2, d3x2.columns());
    ASSERT_EQ(6, d3x2.size());
}

TEST(TestCore_Array2D, CreateEmptyWithDefaultValue) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 4;
    auto data = 5.0;

    auto d2x3v5 = DoubleArray2D(rows, cols, data);

    ASSERT_EQ(rows, d2x3v5.rows());
    ASSERT_EQ(cols, d2x3v5.columns());
    ASSERT_EQ(rows * cols, d2x3v5.size());

    for (auto i = 0; i < rows; i++) {
        for (auto j = 0; j < cols; j++) {
            ASSERT_EQ(data, d2x3v5(i, j));
        }
    }
}

TEST(TestCore_Array2D, CreateFullFromVector) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 4;

    std::vector<int> n = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    auto i3x4v = IntegerArray2D(rows, cols, n);

    ASSERT_EQ(3, i3x4v.rows());
    ASSERT_EQ(4, i3x4v.columns());
    ASSERT_EQ(12, i3x4v.size());
}

TEST(TestCore_Array2D, CreateWithZeroSizeThrows) {
    using namespace hgps::core;

    ASSERT_THROW(IntegerArray2D(0, 5), std::invalid_argument);
    ASSERT_THROW(IntegerArray2D(5, 0), std::invalid_argument);
}

TEST(TestCore_Array2D, CreateWithSizeMismatchThrows) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 3;
    std::vector<int> n = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};

    ASSERT_THROW(IntegerArray2D(rows, cols, n), std::invalid_argument);
}

TEST(TestCore_Array2D, AccessViaColumnIndex) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    std::vector<int> row0 = {2, 1, 5, 7};
    std::vector<int> row1 = {8, 9, 7, 3};
    std::vector<int> row2 = {5, 4, 2, 9};
    auto i3x4v = IntegerArray2D(rows, cols, data);

    for (auto i = 0; i < cols; i++) {
        ASSERT_EQ(row0[i], i3x4v(0, i));
        ASSERT_EQ(row1[i], i3x4v(1, i));
        ASSERT_EQ(row2[i], i3x4v(2, i));
    }
}

TEST(TestCore_Array2D, AccessViaConstColumnIndex) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    std::vector<int> row0 = {2, 1, 5, 7};
    std::vector<int> row1 = {8, 9, 7, 3};
    std::vector<int> row2 = {5, 4, 2, 9};
    const auto i3x4v = IntegerArray2D(rows, cols, data);

    for (auto i = 0; i < cols; i++) {
        ASSERT_EQ(row0[i], i3x4v(0, i));
        ASSERT_EQ(row1[i], i3x4v(1, i));
        ASSERT_EQ(row2[i], i3x4v(2, i));
    }
}

TEST(TestCore_Array2D, AccessViaRowIndex) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    std::vector<int> col0 = {2, 8, 5};
    std::vector<int> col1 = {1, 9, 4};
    std::vector<int> col2 = {5, 7, 2};
    std::vector<int> col3 = {7, 3, 9};
    auto i3x4v = IntegerArray2D(rows, cols, data);

    for (auto i = 0; i < rows; i++) {
        ASSERT_EQ(col0[i], i3x4v(i, 0));
        ASSERT_EQ(col1[i], i3x4v(i, 1));
        ASSERT_EQ(col2[i], i3x4v(i, 2));
        ASSERT_EQ(col3[i], i3x4v(i, 3));
    }
}

TEST(TestCore_Array2D, AccessViaConstRowIndex) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    std::vector<int> col0 = {2, 8, 5};
    std::vector<int> col1 = {1, 9, 4};
    std::vector<int> col2 = {5, 7, 2};
    std::vector<int> col3 = {7, 3, 9};
    const auto i3x4v = IntegerArray2D(rows, cols, data);

    for (auto i = 0; i < rows; i++) {
        ASSERT_EQ(col0[i], i3x4v(i, 0));
        ASSERT_EQ(col1[i], i3x4v(i, 1));
        ASSERT_EQ(col2[i], i3x4v(i, 2));
        ASSERT_EQ(col3[i], i3x4v(i, 3));
    }
}

TEST(TestCore_Array2D, AccessViaRowColumnIndex) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    auto i3x4v = IntegerArray2D(rows, cols, data);

    for (auto i = 0; i < rows; i++) {
        for (auto j = 0; j < cols; j++) {
            auto vector_idx = i * cols + j;
            ASSERT_EQ(data[vector_idx], i3x4v(i, j));
        }
    }
}

TEST(TestCore_Array2D, AccessOutOfRangeThrows) {
    using namespace hgps::core;
    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    auto i3x4v = IntegerArray2D(rows, cols, data);

    ASSERT_THROW(i3x4v(1, 5), std::out_of_range);
    ASSERT_THROW(i3x4v(1, 10), std::out_of_range);
    ASSERT_THROW(i3x4v(5, 2), std::out_of_range);
    ASSERT_THROW(i3x4v(10, 2), std::out_of_range);
}

TEST(TestCore_Array2D, ExportStorageToVector) {
    using namespace hgps::core;

    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    auto i3x4v = IntegerArray2D(rows, cols, data);
    auto i3vec = i3x4v.to_vector();

    ASSERT_EQ(data.size(), i3vec.size());
    ASSERT_EQ(data, i3vec);
}

TEST(TestCore_Array2D, PrintDataToString) {
    using namespace hgps::core;

    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    auto i3x4v = IntegerArray2D(rows, cols, data);
    auto i3str = i3x4v.to_string();

    ASSERT_TRUE(i3str.size() > 10);
    EXPECT_EQ(0, i3str.find_first_of("Array"));
    EXPECT_TRUE(i3str.find_first_of(':') > 0);
    EXPECT_EQ('\n', i3str.back());
}

TEST(TestCore_Array2D, UpdateStorageValue) {
    using namespace hgps::core;

    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    auto i3x4v = IntegerArray2D(rows, cols, data);

    for (auto i = 0; i < rows; i++) {
        for (auto j = 0; j < cols; j++) {
            // Read current
            ASSERT_EQ(data[i * 4 + j], i3x4v(i, j));

            // Update value
            i3x4v(i, j) += 5;

            // Read again
            auto vector_idx = i * cols + j;
            ASSERT_EQ(data[vector_idx] + 5, i3x4v(i, j));
        }
    }
}

TEST(TestCore_Array2D, ClearStorageValue) {
    using namespace hgps::core;

    auto rows = 3;
    auto cols = 4;

    std::vector<int> data = {2, 1, 5, 7, 8, 9, 7, 3, 5, 4, 2, 9};
    auto i3x4v = IntegerArray2D(rows, cols, data);

    i3x4v.clear();
    for (auto i = 0; i < rows; i++) {
        for (auto j = 0; j < cols; j++) {
            ASSERT_EQ(0, i3x4v(i, j));
        }
    }
}
