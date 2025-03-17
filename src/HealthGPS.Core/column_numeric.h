#pragma once

#include "column_primitive.h"
#include <stdexcept>

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
    // Returns owned pointer that must be deleted by caller
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) - Ownership transfer is part of the clone
    // pattern, handled by public clone() method wrapping in unique_ptr
    DataTableColumn *clone_impl() const override { return new StringDataTableColumn(*this); }
};

/// @brief DataTable column for storing float data type
class FloatDataTableColumn final : public PrimitiveDataTableColumn<float> {
  public:
    using PrimitiveDataTableColumn<float>::PrimitiveDataTableColumn;

    std::string type() const noexcept override { return "float"; }

  protected:
    // Returns owned pointer that must be deleted by caller
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) - Ownership transfer is part of the clone
    // pattern, handled by public clone() method wrapping in unique_ptr
    DataTableColumn *clone_impl() const override { return new FloatDataTableColumn(*this); }
};

/// @brief DataTable column for storing double data type
class DoubleDataTableColumn final : public PrimitiveDataTableColumn<double> {
  public:
    using PrimitiveDataTableColumn<double>::PrimitiveDataTableColumn;

    std::string type() const noexcept override { return "double"; }

  protected:
    // Returns owned pointer that must be deleted by caller
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) - Ownership transfer is part of the clone
    // pattern, handled by public clone() method wrapping in unique_ptr
    DataTableColumn *clone_impl() const override { return new DoubleDataTableColumn(*this); }
};

/// @brief DataTable column for storing integer data type
class IntegerDataTableColumn final : public PrimitiveDataTableColumn<int> {
  public:
    IntegerDataTableColumn(const std::string &name, const std::vector<int> &data,
                           const std::vector<bool> &validity = {})
        : PrimitiveDataTableColumn<int>(name, data, validity) {
        // Validate column name
        if (name.empty()) {
            throw std::invalid_argument("Column name cannot be empty");
        }
        if (std::all_of(name.begin(), name.end(), ::isspace)) {
            throw std::invalid_argument("Column name cannot be whitespace-only");
        }
        if (std::any_of(name.begin(), name.end(), [](char c) {
                return c == ' ' || c == '-' || c == '@' || c == '#' || c == '$' || c == '%' ||
                       c == '^' || c == '&' || c == '*';
            })) {
            throw std::invalid_argument("Column name cannot contain spaces or special characters");
        }
    }

    std::string type() const noexcept override { return "integer"; }

  protected:
    // Returns owned pointer that must be deleted by caller
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) - Ownership transfer is part of the clone
    // pattern, handled by public clone() method wrapping in unique_ptr
    DataTableColumn *clone_impl() const override { return new IntegerDataTableColumn(*this); }
};

inline bool is_valid_column_name(const std::string &name) {
    if (name.empty()) {
        return false;
    }
    if (std::all_of(name.begin(), name.end(), ::isspace)) {
        return false;
    }
    return !std::any_of(name.begin(), name.end(), [](char c) {
        return c == ' ' || c == '-' || c == '@' || c == '#' || c == '$' || c == '%' || c == '^' ||
               c == '&' || c == '*';
    });
}
} // namespace hgps::core
