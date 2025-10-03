#pragma once

#include "HealthGPS.Core/disease.h"
#include "HealthGPS.Core/identifier.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace hgps::input {

/// @brief PIF data item for a specific age, gender, and time
struct PIFDataItem {
    int age;
    core::Gender gender;
    int year_post_intervention;
    double pif_value;

    /// @brief Default constructor
    PIFDataItem()
        : age(0), gender(core::Gender::female), year_post_intervention(0), pif_value(0.0) {}

    /// @brief Constructor with parameters
    PIFDataItem(int age, core::Gender gender, int year_post_intervention, double pif_value)
        : age(age), gender(gender), year_post_intervention(year_post_intervention),
          pif_value(pif_value) {}
};

/// @brief PIF data table for a specific disease-risk factor-scenario combination
class PIFTable {
  public:
    /// @brief Default constructor
    PIFTable()
        : min_age_(0), max_age_(0), min_year_(0), max_year_(0), age_range_(1), year_range_(1) {}

    /// @brief Get PIF value for specific age, gender, and time
    /// @param age Person's age
    /// @param gender Person's gender
    /// @param year_post_intervention Years since intervention start
    /// @return PIF value (0.0 to 1.0), returns 0.0 if not found
    double get_pif_value(int age, core::Gender gender, int year_post_intervention) const;

    /// @brief Add PIF data item
    /// @param item PIF data item to add
    void add_item(const PIFDataItem &item);

    /// @brief Build hash table from vector data for direct access
    /// Call this after adding all items to optimize lookup performance
    void build_hash_table();

    /// @brief Check if PIF data is available
    /// @return true if data is available, false otherwise
    bool has_data() const noexcept { return !data_.empty(); }

    /// @brief Get the number of data items
    /// @return Number of PIF data items
    std::size_t size() const noexcept { return data_.size(); }

    /// @brief Clear all data
    void clear() noexcept {
        data_.clear();
        direct_array_.clear();
    }

  private:
    std::vector<PIFDataItem> data_;

    // Direct array for TRUE O(1) access - no lookups!
    std::vector<PIFDataItem> direct_array_;

    int min_age_, max_age_, min_year_{0}, max_year_;
    int age_range_, year_range_{1};
};

/// @brief PIF data container for multiple scenarios
class PIFData {
  public:
    /// @brief Default constructor
    PIFData() = default;

    /// @brief Add PIF table for a specific scenario
    /// @param scenario Scenario name (e.g., "Scenario1", "Scenario2", "Scenario3")
    /// @param table PIF table to add
    void add_scenario_data(const std::string &scenario, PIFTable &&table);

    /// @brief Get PIF table for specific scenario
    /// @param scenario Scenario name
    /// @return Pointer to PIF table, or nullptr if not found
    const PIFTable *get_scenario_data(const std::string &scenario) const;

    /// @brief Check if PIF data is available for any scenario
    /// @return true if data is available, false otherwise
    bool has_data() const noexcept { return !scenario_data_.empty(); }

    /// @brief Get the number of scenarios
    /// @return Number of scenarios with data
    std::size_t scenario_count() const noexcept { return scenario_data_.size(); }

    /// @brief Get all available scenario names
    /// @return Vector of scenario names
    std::vector<std::string> get_scenario_names() const;

    /// @brief Clear all data
    void clear() noexcept { scenario_data_.clear(); }

  private:
    std::unordered_map<std::string, PIFTable> scenario_data_;
};

} // namespace hgps::input
