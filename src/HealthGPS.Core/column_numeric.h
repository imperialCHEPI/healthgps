#pragma once

#include "column_primitive.h"

namespace hgps::core {

/// @brief DataTable column for storing string data type
class StringDataTableColumn final : public PrimitiveDataTableColumn<std::string> {
  public:
    /// @brief Inherit constructors from base class
    using PrimitiveDataTableColumn<std::string>::PrimitiveDataTableColumn;

    /// @brief Get the type name
    /// @return Always returns "string"
    std::string type() const noexcept override { return "string"; }

  protected:
    DataTableColumn* clone_impl() const override {
        return new StringDataTableColumn(*this);
    }
};

/// @brief DataTable column for storing float data type
class FloatDataTableColumn final : public PrimitiveDataTableColumn<float> {
  public:
    using PrimitiveDataTableColumn<float>::PrimitiveDataTableColumn;

    std::string type() const noexcept override { return "float"; }

  protected:
    DataTableColumn* clone_impl() const override {
        return new FloatDataTableColumn(*this);
    }
};

/// @brief DataTable column for storing double data type
class DoubleDataTableColumn final : public PrimitiveDataTableColumn<double> {
  public:
    using PrimitiveDataTableColumn<double>::PrimitiveDataTableColumn;

    std::string type() const noexcept override { return "double"; }

  protected:
    DataTableColumn* clone_impl() const override {
        return new DoubleDataTableColumn(*this);
    }
};

/// @brief DataTable column for storing integer data type
class IntegerDataTableColumn final : public PrimitiveDataTableColumn<int> {
  public:
    using PrimitiveDataTableColumn<int>::PrimitiveDataTableColumn;

    std::string type() const noexcept override { return "integer"; }

  protected:
    DataTableColumn* clone_impl() const override {
        return new IntegerDataTableColumn(*this);
    }
};
} // namespace hgps::core
