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

    /// @brief Lower middle income (4-category system)
    lowermiddle,

    /// @brief Middle income (3-category system)
    middle,

    /// @brief Upper middle income (4-category system)
    uppermiddle,

    /// @brief High income
    high
};

// Note: Region and Ethnicity are now dynamic string-based identifiers
// They are read from CSV files and can support any number of categories
// The system will automatically adapt to the CSV structure

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
