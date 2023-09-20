#pragma once

#include "column_primitive.h"

namespace hgps::core {

/// @brief DataTable column for storing @c float data type class.
class FloatDataTableColumn : public PrimitiveDataTableColumn<float> {
  public:
    using PrimitiveDataTableColumn<float>::PrimitiveDataTableColumn;

    void accept(DataTableColumnVisitor &visitor) const override { visitor.visit(*this); }
};

/// @brief DataTable column for storing @c double data type class.
class DoubleDataTableColumn : public PrimitiveDataTableColumn<double> {
  public:
    using PrimitiveDataTableColumn<double>::PrimitiveDataTableColumn;

    void accept(DataTableColumnVisitor &visitor) const override { visitor.visit(*this); }
};

/// @brief DataTable column for storing @c integer data type class.
class IntegerDataTableColumn : public PrimitiveDataTableColumn<int> {
  public:
    using PrimitiveDataTableColumn<int>::PrimitiveDataTableColumn;

    void accept(DataTableColumnVisitor &visitor) const override { visitor.visit(*this); }
};
} // namespace hgps::core
