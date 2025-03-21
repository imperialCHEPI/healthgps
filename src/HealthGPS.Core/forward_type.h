#pragma once
#include <cstdint>
#include <type_traits>

// forward type declaration
namespace hgps::core {

/// @brief Verbosity mode enumeration
enum class VerboseMode : uint8_t {
    /// @brief only report errors
    none,

    /// @brief Print more information about actions, including warning
    verbose
};

/// @brief Enumerates gender types
enum class Gender : uint8_t {
    /// @brief Unknown gender
    unknown,

    /// @brief Male
    male,

    /// @brief Female
    female
};

/// @brief Enumerates supported diseases types
enum class DiseaseGroup : uint8_t {
    /// @brief Common diseases excluding cancers
    other,

    /// @brief Cancer disease
    cancer
};

/// @brief Enumerates sector types
enum class Sector : uint8_t {
    /// @brief Unknown sector
    unknown,

    /// @brief Urban sector
    urban,

    /// @brief Rural sector
    rural
};

/// @brief Enumerates income categories
enum class Income : uint8_t {
    /// @brief Unknown income
    unknown,

    /// @brief Low income
    low,

    /// @brief Lower middle income
    lowermiddle,

    /// @brief Upper middle income
    uppermiddle,

    /// @brief High income
    high
};

/// @brief Enumerates region categories
enum class Region : uint8_t {
    /// @brief Unknown region
    unknown,

    /// @brief England
    England,

    /// @brief Wales
    Wales,

    /// @brief Scotland
    Scotland,

    /// @brief Northern Ireland
    NorthernIreland
};

/// @brief C++20 concept for numeric columns types
template <typename T>
concept Numerical = std::is_arithmetic_v<T>;

/// @brief Lookup table entry for gender values
struct LookupGenderValue {
    /// @brief Lookup value
    int value{};

    /// @brief The males value
    double male{};

    /// @brief The females value
    double female{};
};

class DataTable;
class DataTableColumn;

class StringDataTableColumn;
class FloatDataTableColumn;
class DoubleDataTableColumn;
class IntegerDataTableColumn;

class DataTableColumnVisitor;

} // namespace hgps::core
