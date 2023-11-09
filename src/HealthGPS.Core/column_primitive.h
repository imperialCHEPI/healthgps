#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "column.h"
#include "column_iterator.h"

namespace hgps::core {

/// @brief Primitive data type DataTable columns class.
/// @tparam TYPE Column data type
template <typename TYPE> class PrimitiveDataTableColumn : public DataTableColumn {
  public:
    using value_type = TYPE;
    using IteratorType = DataTableColumnIterator<PrimitiveDataTableColumn<TYPE>>;

    /// @brief Initialises a new instance of the PrimitiveDataTableColumn class.
    /// @param name Column name
    /// @param data Column data
    /// @throws std::invalid_argument for column name starting with non-alpha character or less than
    /// two length.
    explicit PrimitiveDataTableColumn(std::string &&name, std::vector<TYPE> &&data)
        : DataTableColumn(), name_{name}, data_{data} {
        if (name_.length() < 2 || !std::isalpha(name_.front())) {
            throw std::invalid_argument(
                "Invalid column name: minimum length of two and start with alpha character.");
        }

        null_count_ = 0;
    }

    /// @brief Initialises a new instance of the PrimitiveDataTableColumn class.
    /// @param name Column name
    /// @param data Column data, which may contain null values
    /// @param null_bitmap Column null values index
    /// @throws std::invalid_argument for column name starting with non-alpha character or less than
    /// two length.
    /// @throws std::out_of_range for data and null bitmap vectors size mismatch.
    explicit PrimitiveDataTableColumn(std::string &&name, std::vector<TYPE> &&data,
                                      std::vector<bool> &&null_bitmap)
        : DataTableColumn(), name_{name}, data_{data}, null_bitmap_{null_bitmap} {
        if (name_.length() < 2 || !std::isalpha(name_.front())) {
            throw std::invalid_argument(
                "Invalid column name: minimum length of two and start with alpha character.");
        }

        if (data_.size() != null_bitmap_.size()) {
            throw std::out_of_range(
                "Input vectors size mismatch, the data and valid vectors size must be the same.");
        }

        null_count_ = std::count(null_bitmap_.begin(), null_bitmap_.end(), false);
    }

    std::string type() const noexcept override { return typeid(TYPE).name(); }

    std::string name() const noexcept override { return name_; }

    std::size_t null_count() const noexcept override { return null_count_; }

    std::size_t size() const noexcept override { return data_.size(); }

    bool is_null(std::size_t index) const noexcept override {
        // Bound checks hit performance!
        if (index >= size()) {
            return true;
        }

        return !null_bitmap_.empty() && !null_bitmap_[index];
    }

    bool is_valid(std::size_t index) const noexcept override {
        // Bound checks hit performance!
        if (index >= size()) {
            return false;
        }

        return null_bitmap_.empty() || null_bitmap_[index];
    }

    const std::any value(std::size_t index) const noexcept override {
        if (is_valid(index)) {
            return data_[index];
        }

        return std::any();
    }

    /// @brief Gets the column value at a given index.
    /// @param index Column index
    /// @return The value at index, if inside bounds; otherwise empty
    const std::optional<value_type> value_safe(const std::size_t index) const noexcept {
        if (is_valid(index)) {
            return data_[index];
        }

        return std::nullopt;
    }

    /// @brief Gets the column value at a given index, unsafe without boundary checks.
    /// @warning Accessing a column element outside the boundaries is undefined behaviour.
    /// @param index Column index
    /// @return The value at index
    const value_type value_unsafe(const std::size_t index) const { return data_[index]; }

    /// @brief Gets the iterator to the first element of the column.
    /// @return An iterator to the beginning
    IteratorType begin() const { return IteratorType(*this); }

    /// @brief Gets the iterator element following the last element of the column.
    /// @return An iterator to the end
    IteratorType end() const { return IteratorType(*this, size()); }

    /// Returns the underlying column data as a vector.
    const std::vector<value_type> &values() const { return data_; }

  private:
    std::string name_;
    std::vector<TYPE> data_;
    std::vector<bool> null_bitmap_{};
    std::size_t null_count_ = 0;
};

/// @brief DataTable column for storing @c string data type class.
class StringDataTableColumn : public PrimitiveDataTableColumn<std::string> {
  public:
    using PrimitiveDataTableColumn<std::string>::PrimitiveDataTableColumn;

    std::string type() const noexcept override { return "string"; }

    void accept(DataTableColumnVisitor &visitor) const override { visitor.visit(*this); }
};
} // namespace hgps::core
