#pragma once
#include <exception>
#include <limits>
#include <memory>
#include <vector>

#include "column.h"

namespace hgps::core {

/// @brief Primitive data type DataTable column builder class
/// @tparam ColumnType
template <typename ColumnType> class PrimitiveDataTableColumnBuilder {

  public:
    using value_type = typename ColumnType::value_type;

    PrimitiveDataTableColumnBuilder() = delete;

    /// @brief Initialise a new instance of the PrimitiveDataTableColumnBuilder class.
    /// @param name The column name
    explicit PrimitiveDataTableColumnBuilder(std::string name) : name_{name} {
        if (name_.length() < 2 || !std::isalpha(name_.front())) {
            throw std::invalid_argument(
                "Invalid column name: minimum length of two and start with alpha character.");
        }
    }

    /// @brief Gets the column name
    /// @return Column name
    std::string name() const { return name_; }

    /// @brief Gets the column data size
    /// @return Column size
    std::size_t size() const { return data_.size(); }

    /// @brief Gets the number of null values in the column data
    /// @return Column null values count
    std::size_t null_count() const { return null_count_; }

    /// @brief Gets the builder data capacity
    /// @return Current capacity
    std::size_t capacity() const { return data_.capacity(); }

    /// @brief Reserve builder storage capacity
    /// @param capacity new capacity, in number of elements
    void reserve(std::size_t capacity) { return data_.reserve(capacity); }

    /// @brief Append a single null value
    void append_null() { append_null_internal(1); }

    /// @brief Append multiple null values
    /// @param count The number of null values to append
    void append_null(const std::size_t count) { append_null_internal(count); }

    void append(const value_type value) { append_internal(value); }

    /// @brief Gets the column value at a given index, no boundary checks
    /// @param index Column index
    /// @return The value at index, if inside bounds; otherwise, undefined behaviour.
    value_type value(std::size_t index) const { return data_[index]; }

    /// @brief Gets the column value at a given index, no boundary checks
    /// @param index Column index
    /// @return The value at index, if inside bounds; otherwise, undefined behaviour.
    value_type operator[](std::size_t index) const { return value(index); }

    /// @brief Gets the column value at a given index, no boundary checks
    /// @param index Column index
    /// @return The value at index, if inside bounds; otherwise, undefined behaviour.
    value_type &operator[](std::size_t index) { return data_[index]; }

    /// @brief Resets the column builder data
    void reset() {
        data_.clear();
        null_bitmap_.clear();
        null_count_ = 0;
    }

    /// @brief Builds the column with current data
    /// @return The new column instance
    [[nodiscard]] std::unique_ptr<ColumnType> build() {
        data_.shrink_to_fit();
        null_bitmap_.shrink_to_fit();

        if (null_count_ > 0) {

            // Contains null data, use null bitmap
            return std::make_unique<ColumnType>(std::move(name_), std::move(data_),
                                                std::move(null_bitmap_));
        }

        // Full vector, no need for null bitmap
        return std::make_unique<ColumnType>(std::move(name_), std::move(data_));
    }

  private:
    std::string name_;
    std::vector<value_type> data_{};
    std::vector<bool> null_bitmap_{};
    std::size_t null_count_ = 0;

    void append_null_internal(const std::size_t length) {
        null_count_ += length;
        data_.insert(data_.end(), length, value_type{});
        null_bitmap_.insert(null_bitmap_.end(), length, false);
    }

    void append_internal(const value_type value) {
        data_.push_back(value);
        null_bitmap_.push_back(true);
    }
};

/// @brief Builder for DataTable columns storing @c string data type class
using StringDataTableColumnBuilder = PrimitiveDataTableColumnBuilder<StringDataTableColumn>;

/// @brief Builder for DataTable columns storing @c float data type class
using FloatDataTableColumnBuilder = PrimitiveDataTableColumnBuilder<FloatDataTableColumn>;

/// @brief Builder for DataTable columns storing @c double data type class
using DoubleDataTableColumnBuilder = PrimitiveDataTableColumnBuilder<DoubleDataTableColumn>;

/// @brief Builder for DataTable columns storing @c Integer data type class
using IntegerDataTableColumnBuilder = PrimitiveDataTableColumnBuilder<IntegerDataTableColumn>;
} // namespace hgps::core