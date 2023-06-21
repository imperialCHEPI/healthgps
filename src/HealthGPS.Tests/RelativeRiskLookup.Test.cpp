#include "pch.h"

#include "HealthGPS/relative_risk.h"

hgps::RelativeRiskLookup create_test_relative_risk_table() {
    using namespace hgps;
    using namespace hgps::core;

    auto ages = std::vector<int>{0, 1, 2, 3, 4, 5, 7};
    auto values = std::vector<float>{18.0f, 25.0f, 30.0f, 35.0f, 40.0f};
    std::vector<float> data = {
        1.0f, 4.5f, 8.0f, 11.6f, 15.0f, // 0
        1.0f, 4.4f, 7.9f, 11.3f, 14.8f, // 1
        1.0f, 4.3f, 7.6f, 11.0f, 14.3f, // 2
        1.0f, 4.2f, 7.3f, 10.5f, 13.5f, // 3
        1.0f, 3.8f, 6.7f, 9.5f,  12.4f, // 4
        1.0f, 3.5f, 5.8f, 8.2f,  10.6f, // 5
        1.0f, 2.7f, 4.5f, 6.2f,  9.7f   // 6
    };

    auto lut_rows = MonotonicVector(ages);
    auto lut_cols = MonotonicVector(values);
    return RelativeRiskLookup{lut_rows, lut_cols, FloatArray2D(7, 5, data)};
}

TEST(TestRelativeRiskLookup, CreateEmptyStorage) {
    using namespace hgps;
    using namespace hgps::core;

    auto ages = std::vector<int>{0};
    auto values = std::vector<float>{0};

    auto lut_rows = MonotonicVector(ages);
    auto lut_cols = MonotonicVector(values);
    auto lut = RelativeRiskLookup{lut_rows, lut_cols, FloatArray2D(1, 1)};
    ASSERT_EQ(1, lut.rows());
    ASSERT_EQ(1, lut.columns());
    ASSERT_EQ(1, lut.size());
    ASSERT_EQ(0, lut(0, 0));
}

TEST(TestRelativeRiskLookup, ExactValueLookup) {
    using namespace hgps;
    using namespace hgps::core;

    auto expected_rows = 7;
    auto expected_cols = 5;
    auto expected_size = expected_rows * expected_cols;
    auto expected_age3 = std::vector<float>{1.0f, 4.2f, 7.3f, 10.5f, 13.5f};
    auto expected_age5 = std::vector<float>{1.0f, 3.5f, 5.8f, 8.2f, 10.6f};

    auto lut = create_test_relative_risk_table();

    auto age3 = 3;
    auto age5 = 5;
    auto cols = std::vector<float>{18.0f, 25.0f, 30.0f, 35.0f, 40.0f};

    ASSERT_EQ(expected_rows, lut.rows());
    ASSERT_EQ(expected_cols, lut.columns());
    ASSERT_EQ(expected_size, lut.size());
    for (size_t i = 0; i < cols.size(); i++) {
        ASSERT_EQ(expected_age3[i], lut(age3, cols[i]));
        ASSERT_EQ(expected_age5[i], lut(age5, cols[i]));
    }
}

TEST(TestRelativeRiskLookup, InterpolateValueLookup) {
    using namespace hgps;
    using namespace hgps::core;

    auto expected_rows = 7;
    auto expected_cols = 5;
    auto expected_size = expected_rows * expected_cols;
    auto expected_row3 = std::vector<float>{1.0f, 4.2f, 7.3f, 10.5f, 13.5f};

    auto test_age = 3;
    auto test_values = std::vector<float>{15.0f, 21.0, 27.5f, 32.5f, 38.0f, 42.0f};

    auto lut_cols = std::vector<float>{18.0f, 25.0f, 30.0f, 35.0f, 40.0f};
    auto lut = create_test_relative_risk_table();

    ASSERT_EQ(expected_rows, lut.rows());
    ASSERT_EQ(expected_cols, lut.columns());
    ASSERT_EQ(expected_size, lut.size());
    for (size_t i = 0; i < test_values.size(); i++) {
        auto value = test_values[i];
        auto lut_value = lut(test_age, value);
        if (value <= lut_cols.front()) {
            ASSERT_EQ(expected_row3.front(), lut_value);
        } else if (value >= lut_cols.back()) {
            ASSERT_EQ(expected_row3.back(), lut_value);
        } else {
            auto it = std::find_if(lut_cols.begin(), lut_cols.end(),
                                   [&value](auto v) { return value < v; });
            ASSERT_NE(it, lut_cols.end());
            auto index = std::distance(lut_cols.begin(), it);
            ASSERT_LT(expected_row3[index - 1], lut_value);
            ASSERT_GT(expected_row3[index], lut_value);
        }
    }
}

TEST(TestRelativeRiskLookup, ReferenceDataLookup) {
    auto test_age = 3;
    auto test_data = std::map<double, double>{{16.0, 1.00},
                                              {16.5, 1.00},
                                              {17.0, 1.0},
                                              {17.5, 1.00},
                                              {18.0, 1.00},
                                              {18.5, 1.2285714285714286},
                                              {19.0, 1.4571428571428573},
                                              {19.5, 1.6857142857142859},
                                              {20.0, 1.9142857142857144},
                                              {20.5, 2.1428571428571428},
                                              {21.0, 2.3714285714285719},
                                              {21.5, 2.60},
                                              {22.0, 2.8285714285714287},
                                              {22.5, 3.0571428571428574},
                                              {23.0, 3.2857142857142856},
                                              {23.5, 3.5142857142857147},
                                              {24.0, 3.7428571428571433},
                                              {24.5, 3.9714285714285715},
                                              {25.0, 4.2},
                                              {25.5, 4.51},
                                              {26.0, 4.82},
                                              {26.5, 5.13},
                                              {27.0, 5.440},
                                              {27.5, 5.75},
                                              {28.0, 6.06},
                                              {28.5, 6.37},
                                              {29.0, 6.68},
                                              {29.5, 6.99},
                                              {30.0, 7.30},
                                              {30.5, 7.62},
                                              {31.0, 7.94},
                                              {31.5, 8.26},
                                              {32.0, 8.58},
                                              {32.5, 8.90},
                                              {33.0, 9.22},
                                              {33.5, 9.54},
                                              {34.0, 9.86},
                                              {34.5, 10.18},
                                              {35.0, 10.5},
                                              {35.5, 10.80},
                                              {36.0, 11.10},
                                              {36.5, 11.40},
                                              {37.0, 11.7},
                                              {37.5, 12.00},
                                              {38.0, 12.3},
                                              {38.5, 12.60},
                                              {39.0, 12.90},
                                              {39.5, 13.20},
                                              {40.0, 13.5},
                                              {40.5, 13.50},
                                              {41.0, 13.5},
                                              {41.5, 13.50}};

    auto lut = create_test_relative_risk_table();
    for (auto &item : test_data) {
        auto lut_value = lut(test_age, static_cast<float>(item.first));
        ASSERT_FLOAT_EQ(static_cast<float>(item.second), lut_value);
    }
}