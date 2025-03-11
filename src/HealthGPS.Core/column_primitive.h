#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "column.h"
#include "column_iterator.h"
#include "forward_type.h"
#include "visitor.h"

namespace hgps::core {

/// @brief Primitive data type DataTable columns class.
/// @tparam TYPE Column data type
template <typename TYPE> class PrimitiveDataTableColumn : public DataTableColumn {
  public:
    using value_type = TYPE;
    using IteratorType = DataTableColumnIterator<PrimitiveDataTableColumn<TYPE>>;

    /// @brief Default constructor deleted to prevent empty columns
    PrimitiveDataTableColumn() = delete;

    /// @brief Copy constructor
    PrimitiveDataTableColumn(const PrimitiveDataTableColumn &other)
        : name_(other.name_), data_(other.data_), null_bitmap_(other.null_bitmap_),
          null_count_(other.null_count_) {}

    /// @brief Move constructor
    PrimitiveDataTableColumn(PrimitiveDataTableColumn &&other) noexcept
        : name_(std::move(other.name_)), data_(std::move(other.data_)),
          null_bitmap_(std::move(other.null_bitmap_)), null_count_(other.null_count_) {}

    /// @brief Initialises a new instance with name and data
    /// @param name Column name
    /// @param data Column data
    /// @throws std::invalid_argument for invalid column name
    PrimitiveDataTableColumn(std::string name, std::vector<TYPE> data)
        : name_(std::move(name)), data_(std::move(data)) {
        validate_name();
        // Initialize an empty null_bitmap where all values are valid (not null)
        null_bitmap_.resize(data_.size(), false);
        null_count_ = 0;
    }

    /// @brief Initialises a new instance with name, data and null bitmap
    /// @param name Column name
    /// @param data Column data
    /// @param null_bitmap Column null values index (true means value is null)
    /// @throws std::invalid_argument for invalid column name
    /// @throws std::out_of_range for size mismatch
    PrimitiveDataTableColumn(std::string name, std::vector<TYPE> data,
                             std::vector<bool> null_bitmap)
        : name_(std::move(name)), data_(std::move(data)), null_bitmap_(std::move(null_bitmap)) {
        validate_name();
        validate_sizes();
        // Count null values (true in the bitmap means null)
        null_count_ = std::count(null_bitmap_.begin(), null_bitmap_.end(), true);
        // If null_bitmap is empty, initialize it with all values valid
        if (null_bitmap_.empty()) {
            null_bitmap_.resize(data_.size(), false);
        }
    }

    /// @brief Creates a deep copy of this column
    /// @return A unique pointer to the new column copy
    std::unique_ptr<DataTableColumn> clone() const override {
        // Create a copy of this object using its actual type
        return std::unique_ptr<DataTableColumn>(this->clone_impl());
    }

    /// @brief Protected virtual clone implementation
    /// @return A pointer to the cloned object
    virtual DataTableColumn *clone_impl() const = 0;

    /// @brief Get the column data type name
    std::string type() const noexcept override = 0;

    std::string name() const noexcept override { return name_; }

    std::size_t null_count() const noexcept override { return null_count_; }

    std::size_t size() const noexcept override { return data_.size(); }

    bool is_null(std::size_t index) const noexcept override {
        if (index >= size()) {
            return true; // Out of bounds indices are treated as null
        }
        return null_bitmap_[index];
    }

    bool is_valid(std::size_t index) const noexcept override {
        if (index >= size()) {
            return false; // Out of bounds indices are not valid
        }
        return !null_bitmap_[index];
    }

    const std::any value(std::size_t index) const noexcept override {
        if (index >= size()) {
            return std::any(); // Return empty for out of bounds
        }
        if (null_bitmap_[index]) {
            return std::any(); // Return empty for null values
        }
        return data_[index];
    }

    /// @brief Gets the column value at a given index.
    /// @param index Column index
    /// @return The value at index, if inside bounds; otherwise empty
    const std::optional<value_type> value_safe(const std::size_t index) const noexcept {
        if (index >= size() || null_bitmap_[index]) {
            return std::nullopt;
        }
        return data_[index];
    }

    /// @brief Gets the column value at a given index, unsafe without boundary checks.
    /// @param index Column index
    /// @return The value at index
    const value_type &value_unsafe(const std::size_t index) const { return data_[index]; }

    /// @brief Gets the iterator to the first element of the column.
    /// @return An iterator to the beginning
    IteratorType begin() const { return IteratorType(*this); }

    /// @brief Gets the iterator element following the last element of the column.
    /// @return An iterator to the end
    IteratorType end() const { return IteratorType(*this, size()); }

    /// @brief Accept method implementation for visitor pattern
    /// @param visitor The visitor instance
    void accept(DataTableColumnVisitor &visitor) const override {
        if constexpr (std::is_same_v<TYPE, std::string>) {
            visitor.visit(static_cast<const StringDataTableColumn &>(*this));
        } else if constexpr (std::is_same_v<TYPE, int>) {
            visitor.visit(static_cast<const IntegerDataTableColumn &>(*this));
        } else if constexpr (std::is_same_v<TYPE, float>) {
            visitor.visit(static_cast<const FloatDataTableColumn &>(*this));
        } else if constexpr (std::is_same_v<TYPE, double>) {
            visitor.visit(static_cast<const DoubleDataTableColumn &>(*this));
        } else {
            // This will cause a compile error for unsupported types
            static_assert(
                std::is_same_v<TYPE, void>,
                "PrimitiveDataTableColumn only supports string, int, float, and double types");
        }
    }

    /// @brief Check if column has a value at the given index
    /// @param index Index to check
    /// @return True if index is in bounds and value is not null
    bool has_value(std::size_t index) const noexcept {
        if (index >= size()) {
            return false;
        }
        return !null_bitmap_[index];
    }

  protected:
    /// @brief Validates the column name
    void validate_name() const {
        if (name_.length() < 2 || !std::isalpha(name_.front())) {
            throw std::invalid_argument(
                "Invalid column name: minimum length of two and start with alpha character.");
        }
    }

    /// @brief Validates data and null bitmap sizes match
    void validate_sizes() const {
        if (!null_bitmap_.empty() && data_.size() != null_bitmap_.size()) {
            throw std::out_of_range(
                "Input vectors size mismatch, the data and valid vectors size must be the same.");
        }
    }

  private:
    std::string name_;
    std::vector<TYPE> data_;
    std::vector<bool> null_bitmap_{};
    std::size_t null_count_ = 0;
};

} // namespace hgps::core
