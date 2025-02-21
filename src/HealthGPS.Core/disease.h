#pragma once

#include <map>
#include <string>
#include <vector>

#include "forward_type.h"
#include "identifier.h"

namespace hgps::core {

/// @brief Disease information structure
struct DiseaseInfo {
    /// @brief The disease group (i.e. cancer or non-cancer)
    DiseaseGroup group{};

    /// @brief Unique identifier
    Identifier code{};

    /// @brief Disease full name
    std::string name{};
};

/// @brief Determine whether a specified DiseaseInfo is less than another instance.
/// @param lhs The first instance to compare.
/// @param rhs The second instance to compare.
/// @return true if left instance is less than right instance; otherwise, false.
inline bool operator<(const DiseaseInfo &lhs, const DiseaseInfo &rhs) {
    return lhs.name < rhs.name;
}

/// @brief Determine whether a specified DiseaseInfo is greater than another instance.
/// @param lhs The first instance to compare.
/// @param rhs The second instance to compare.
/// @return true if left instance is greater than right instance; otherwise, false.
inline bool operator>(const DiseaseInfo &lhs, const DiseaseInfo &rhs) {
    return lhs.name > rhs.name;
}

/// @brief Disease data item structure
struct DiseaseItem {
    /// @brief Item age reference
    int with_age;

    /// @brief Gender identifier
    Gender gender;

    /// @brief Associated data measure values
    std::map<int, double> measures;
};

/// @brief Disease full definition data structure
struct DiseaseEntity {
    /// @brief The associate country
    Country country;

    /// @brief Disease information
    DiseaseInfo info;

    /// @brief Data measures lookup by name
    std::map<std::string, int> measures;

    /// @brief The collection of data measure items
    std::vector<DiseaseItem> items;

    /// @brief Checks whether the disease data definition is empty
    /// @return true for an empty definition, otherwise false.
    bool empty() const noexcept { return measures.empty() || items.empty(); }
};

/// @brief Diseases relative risk effect table data structure
struct RelativeRiskEntity {
    /// @brief The table column identifiers
    std::vector<std::string> columns;

    /// @brief The table rows with data for each column
    std::vector<std::vector<float>> rows;

    /// @brief Checks whether the relative risk table data is empty
    /// @return true for an empty table, otherwise false.
    bool empty() const noexcept { return rows.empty(); }
};

/// @brief Cancer disease parameters per country data structure
struct CancerParameterEntity {
    /// @brief The reference time
    int at_time{};

    /// @brief The cancer death weight values
    std::vector<LookupGenderValue> death_weight{};

    /// @brief The cancer prevalence distribution
    std::vector<LookupGenderValue> prevalence_distribution{};

    /// @brief The cancer survival rates
    std::vector<LookupGenderValue> survival_rate{};

    /// @brief Checks whether the cancer parameters data is empty
    /// @return true for an empty definition, otherwise false.
    bool empty() const noexcept {
        return death_weight.empty() || prevalence_distribution.empty() || survival_rate.empty();
    }
};
} // namespace hgps::core
