#pragma once

#include "forward_type.h"

namespace hgps::core {

/// @brief DataTable column visitor interface
class DataTableColumnVisitor {
  public:
    /// @brief Initialises a new instance of the DataTableColumnVisitor class.
    DataTableColumnVisitor() = default;

    DataTableColumnVisitor(const DataTableColumnVisitor &) = delete;
    DataTableColumnVisitor &operator=(const DataTableColumnVisitor &) = delete;

    DataTableColumnVisitor(DataTableColumnVisitor &&) = delete;
    DataTableColumnVisitor &operator=(DataTableColumnVisitor &&) = delete;

    /// @brief Destroys a DataTableColumnVisitor instance
    virtual ~DataTableColumnVisitor(){};

    /// @brief Visits a column of StringDataTableColumn type
    /// @param column The column instance
    virtual void visit(const StringDataTableColumn &column) = 0;

    /// @brief Visits a column of FloatDataTableColumn type
    /// @param column The column instance
    virtual void visit(const FloatDataTableColumn &column) = 0;

    /// @brief Visits a column of DoubleDataTableColumn type
    /// @param column The column instance
    virtual void visit(const DoubleDataTableColumn &column) = 0;

    /// @brief Visits a column of IntegerDataTableColumn type
    /// @param column The column instance
    virtual void visit(const IntegerDataTableColumn &column) = 0;
};
} // namespace hgps::core