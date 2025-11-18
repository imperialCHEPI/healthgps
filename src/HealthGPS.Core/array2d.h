#pragma once
#include "forward_type.h"

#include <algorithm>
#include <fmt/format.h>
#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hgps::core {

/// @brief Defines a contiguous storage for two-dimensional numerical data in row-major format
/// @tparam TYPE The numerical type to store
template <Numerical TYPE> class Array2D {
  public:
    /// @brief Initialises new instance of the Array2D class with no storage.
    Array2D() = default;

    /// @brief Initialises new instance of the Array2D class.
    /// @param nrows The number of rows in the array
    /// @param ncols The number fo columns in the array
    /// @throws std::invalid_argument for array with zero size storage;
    Array2D(const size_t nrows, const size_t ncols)
        : rows_{nrows}, columns_{ncols}, data_(nrows * ncols) {
        if (nrows <= 0 || ncols <= 0) {
            throw std::invalid_argument("Invalid array constructor with 0 size");
        }

        data_.shrink_to_fit();
    }

    /// @brief Initialises new instance of the Array2D class with custom default value.
    /// @param nrows The number of rows in the array
    /// @param ncols The number of columns in the array
    /// @param value The default value to initialise the array values
    /// @throws std::invalid_argument for array with zero size storage;
    Array2D(const size_t nrows, const size_t ncols, TYPE value) : Array2D(nrows, ncols) {
        fill(value);
    }

    /// @brief Initialises new instance of the Array2D class with custom values.
    /// @param nrows The number of rows in the array
    /// @param ncols The number of columns in the array
    /// @param values Array of values to initialise the storage
    /// @throws std::invalid_argument for array size and values size mismatch.
    Array2D(const size_t nrows, const size_t ncols, std::vector<TYPE> &values)
        : Array2D(nrows, ncols) {

        if (values.size() != size()) {
            throw std::invalid_argument(fmt::format(
                "Array size and values size mismatch: {} vs. given {}.", size(), values.size()));
        }

        std::copy(values.begin(), values.end(), data_.begin());
    }

    /// @brief Gets the total size of the array storage
    /// @return The total size
    size_t size() const noexcept { return rows_ * columns_; }

    /// @brief Gets the number of rows in the array
    /// @return The number of rows
    size_t rows() const noexcept { return rows_; }

    /// @brief Gets the number of coluns in the array
    /// @return The number of columns
    size_t columns() const noexcept { return columns_; }

    /// @brief Gets a value from the array
    /// @param row The zero-based row index
    /// @param column The zero-based columns index
    /// @return The respective value at location
    /// throws std::out_of_range for row or column index out of array bounds
    TYPE &operator()(size_t row, size_t column) {
        check_boundaries(row, column);
        return data_[row * columns_ + column];
    }

    /// @brief Gets a read-only value from the array
    /// @param row The zero-based row index
    /// @param column The zero-based columns index
    /// @return The respective value at location
    /// throws std::out_of_range for row or column index out of array bounds
    const TYPE &operator()(size_t row, size_t column) const {
        check_boundaries(row, column);
        return data_[row * columns_ + column];
    }

    /// @brief Fill the array data with a default value
    /// @param value The default value to store in the array
    void fill(TYPE value) { std::fill(data_.begin(), data_.end(), value); }

    /// @brief Clears the array data with the default type value
    void clear() { fill(TYPE{}); }

    /// @brief Creates a vector copy of the array data
    /// @return The vector with a copy of the data
    std::vector<TYPE> to_vector() { return std::vector<TYPE>(data_); }

    /// @brief Creates a string representation of the array data
    /// @return The string representation
    std::string to_string() noexcept {
        std::stringstream ss;
        ss << "Array: " << rows_ << "x" << columns_ << "\n";
        for (size_t i = 0; i < rows_; i++) {
            ss << "{";
            for (size_t j = 0; j < columns_; j++) {
                ss << operator()(i, j);
                if ((j + 1) != columns_)
                    ss << ", ";
            }

            ss << "}\n";
        }

        return ss.str();
    }

  private:
    size_t rows_{};
    size_t columns_{};
    std::vector<TYPE> data_;

    void check_boundaries(size_t row, size_t column) const {
        if (row >= rows_) {
            // DEBUG: Print detailed error information
            printf("[ARRAY2D ERROR] Row %zu is out of array bounds [0, %zu).\n", row, rows_);
            printf("  Array size: %zu rows x %zu columns\n", rows_, columns_);
            printf("  Attempted access: row=%zu, column=%zu\n", row, column);
            fflush(stdout);
            throw std::out_of_range(
                fmt::format("Row {} is out of array bounds [0, {}).", row, rows_));
        }

        if (column >= columns_) {
            printf("[ARRAY2D ERROR] Column %zu is out of array bounds [0, %zu).\n", column, columns_);
            fflush(stdout);
            throw std::out_of_range(
                fmt::format("Column {} is out of array bounds [0, {}).", column, columns_));
        }
    }
};

/// @brief Contiguous storage for two-dimensional single precision values
using FloatArray2D = Array2D<float>;

/// @brief Contiguous storage for two-dimensional double precision values
using DoubleArray2D = Array2D<double>;

/// @brief Contiguous storage for two-dimensional integer values
using IntegerArray2D = Array2D<int>;
} // namespace hgps::core
