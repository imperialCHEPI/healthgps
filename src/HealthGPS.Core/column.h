#pragma once
#include <any>
#include <memory>
#include <string>
#include <typeinfo>

#include "visitor.h"

namespace hgps::core {

/// @brief DataTable columns interface data type
class DataTableColumn {
  public:
    /// @brief Initialises a new instance of the DataTableColumn class.
    DataTableColumn() = default;

    /// @brief Virtual destructor
    virtual ~DataTableColumn() = default;

    /// @brief Copy constructor
    DataTableColumn(const DataTableColumn &) = default;
    /// @brief Copy assignment operator
    DataTableColumn &operator=(const DataTableColumn &) = default;

    /// @brief Move constructor
    DataTableColumn(DataTableColumn &&) = default;
    /// @brief Move assignment operator
    DataTableColumn &operator=(DataTableColumn &&) = default;

    /// @brief Gets the column type name
    /// @return Column type name
    virtual std::string type() const noexcept = 0;

    /// @brief Gets the column name identifier
    /// @return Column name
    virtual std::string name() const noexcept = 0;

    /// @brief Gets the number of null values in the column data
    /// @return Column null values count
    virtual std::size_t null_count() const noexcept = 0;

    /// @brief The size of the column data, number of rows
    /// @return Column size
    virtual std::size_t size() const noexcept = 0;

    /// @brief Determine whether a column value is null
    /// @param index Column index to check
    /// @return true is the value is null; otherwise, false.
    virtual bool is_null(std::size_t index) const noexcept = 0;

    /// @brief Determine whether a column value is not null
    /// @param index Column index to check
    /// @return true is the value is not null; otherwise, false.
    virtual bool is_valid(std::size_t index) const noexcept = 0;

    /// @brief Gets the column value at a given index
    /// @param index Column index
    /// @return The value at index, if inside bounds; otherwise empty
    virtual const std::any value(std::size_t index) const noexcept = 0;

    /// @brief Double dispatch the column using a visitor implementation
    /// @param visitor The visitor instance to accept
    virtual void accept(DataTableColumnVisitor &visitor) const = 0;

    /// @brief Creates a deep copy of this column
    /// @return A unique pointer to the new column copy
    virtual std::unique_ptr<DataTableColumn> clone() const = 0;
};
} // namespace hgps::core
