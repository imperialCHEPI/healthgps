#pragma once
#include <map>
#include <string>

#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/poco.h"

namespace hgps {

/// @brief Disease measures key mapping
struct MeasureKey final {
    static inline const std::string prevalence{"prevalence"};
    static inline const std::string mortality{"mortality"};
    static inline const std::string remission{"remission"};
    static inline const std::string incidence{"incidence"};
};

/// @brief Defines the disease measures collection data type
/// @details Indirectly lookup by integer instead of the string keys.
class DiseaseMeasure {
  public:
    /// @brief Initialises a new instance of the DiseaseMeasure class.
    DiseaseMeasure() = default;

    /// @brief Initialises a new instance of the DiseaseMeasure class.
    /// @param measures The disease measures value mapping
    DiseaseMeasure(const std::map<int, double> &measures);

    /// @brief Gets the size of the measure collection
    /// @return
    std::size_t size() const noexcept;

    /// @brief Gets the measure value by identifier
    /// @param measure_id Measure identifier
    /// @return Measure value
    /// @throws std::out_of_range for unknown measure identifier
    const double &at(int measure_id) const;

    /// @brief Gets the measure value by identifier
    /// @param measure_id Measure identifier
    /// @return Measure value
    /// @throws std::out_of_range for unknown measure identifier
    const double &operator[](int measure_id) const;

  private:
    std::map<int, double> measures_;
};

/// @brief Defines the disease measure table data type
/// @details Lookup table by age and gender to access multiple disease measures.
class DiseaseTable {
  public:
    DiseaseTable() = delete;

    /// @brief Initialise a new instance of the DiseaseTable class.
    /// @param info The disease information
    /// @param measures Diseases measure to identifier mapping
    /// @param data Disease measure table dataset
    DiseaseTable(const core::DiseaseInfo &info, std::map<std::string, int> &&measures,
                 std::map<int, std::map<core::Gender, DiseaseMeasure>> &&data);

    /// @brief Gets the disease information
    /// @return Disease information
    const core::DiseaseInfo &info() const noexcept;

    /// @brief Gets the total size of the table, number of measure cells
    /// @return Total table size
    std::size_t size() const noexcept;

    /// @brief Gets the number of rows, age.
    /// @return Number of rows
    std::size_t rows() const noexcept;

    /// @brief Gets the number of columns, gender
    /// @return NUmber of columns
    std::size_t cols() const noexcept;

    /// @brief Defines whether the table contains a given age
    /// @param age The age to check
    /// @return true if the age entry exists; otherwise, false.
    bool contains(const int age) const noexcept;

    /// @brief Gets the measures to identifier mapping
    /// @return Measures identifiers
    const std::map<std::string, int> &measures() const noexcept;

    /// @brief Gets a measure identifier by name
    /// @param measure The measure name
    /// @return Measure identifier
    /// @throws std::out_of_range for unknown measure name
    const int &at(const std::string &measure) const;

    /// @brief Gets a measure identifier by name
    /// @param measure The measure name
    /// @return Measure identifier
    /// @throws std::out_of_range for unknown measure name
    const int &operator[](const std::string &measure) const;

    /// @brief Lookup for the disease measure entries by age and gender
    /// @param age The age to lookup
    /// @param gender The gender to lookup
    /// @return Disease measures collection
    /// @throws std::out_of_range for unknown age or gender enumeration
    DiseaseMeasure &operator()(const int age, const core::Gender gender);

    /// @brief Lookup for the disease measure entries by age and gender
    /// @param age The age to lookup
    /// @param gender The gender to lookup
    /// @return Disease measures collection
    /// @throws std::out_of_range for unknown age or gender enumeration
    const DiseaseMeasure &operator()(const int age, const core::Gender gender) const;

  private:
    core::DiseaseInfo info_;
    std::map<std::string, int> measures_;
    std::map<int, std::map<core::Gender, DiseaseMeasure>> data_;
};
} // namespace hgps