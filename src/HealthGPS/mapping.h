#pragma once
#include "HealthGPS.Core/identifier.h"
#include "HealthGPS.Core/interval.h"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace hgps {

/// @brief The constant in the regression model presentation identifier
inline const core::Identifier InterceptKey = core::Identifier{"intercept"};

/// @brief Optional Range of doubles data type
using OptionalInterval = std::optional<core::DoubleInterval>;

/// @brief Defines risk factor mapping entry data type
///
/// @details Single risk factor definition, the dynamic factor is included
/// only in the dynamic risk factor model hierarchy, never in the static model.
class MappingEntry {
  public:
    MappingEntry() = delete;

    /// @brief Initialises a new instance of the MappingEntry class
    /// @param name Risk factor name
    /// @param level The hierarchical level
    /// @param range The factor range
    MappingEntry(std::string name, int level, OptionalInterval range = {});

    /// @brief Gets the factor name
    /// @return Factor name
    const std::string &name() const noexcept;

    /// @brief Gets the factor hierarchical level
    /// @return Factor level
    int level() const noexcept;

    /// @brief Gets the factor unique identification
    /// @return Factor identification
    const core::Identifier &key() const noexcept;

    /// @brief Gets the factor allowed values range
    /// @return Factor values range
    const OptionalInterval &range() const noexcept;

    /// @brief Adjusts a value to the factor range, if provided
    /// @param value The value to adjust
    /// @return The bounded value
    double get_bounded_value(const double &value) const noexcept;

  private:
    std::string name_;
    core::Identifier name_key_;
    int level_{};
    OptionalInterval range_;
};

/// @brief Defines the hierarchical model mapping data type
class HierarchicalMapping {
  public:
    /// @brief Hierarchical mappings iterator
    using IteratorType = std::vector<MappingEntry>::iterator;
    /// @brief read-only hierarchical mappings iterator
    using ConstIteratorType = std::vector<MappingEntry>::const_iterator;

    HierarchicalMapping() = delete;

    /// @brief Initialises a new instance of the HierarchicalMapping class
    /// @param mapping The collection of mapping entries
    HierarchicalMapping(std::vector<MappingEntry> &&mapping);

    /// @brief Gets the collection of mapping entries
    /// @return The mapping entries
    const std::vector<MappingEntry> &entries() const noexcept;

    /// @brief Gets the size of the mappings collection
    /// @return The mappings collection size
    std::size_t size() const noexcept;

    /// @brief Gets the maximum hierarchical level
    /// @return Maximum hierarchy level
    int max_level() const noexcept;

    /// @brief Gets a mapping entry by identifier
    /// @param key The factor unique identifier
    /// @return The factor mapping definition
    /// @throws std::out_of_range for identifier not found in the mappings collection.
    MappingEntry at(const core::Identifier &key) const;

    /// @brief Gets the mapping entries at a given hierarchical level
    /// @param level The hierarchical level
    /// @return The collection of mappings
    std::vector<MappingEntry> at_level(int level) const noexcept;

    /// @brief Gets an iterator to the beginning of the mappings
    /// @return Iterator to the first mapping
    IteratorType begin() noexcept { return mapping_.begin(); }

    /// @brief Gets an iterator to the element following the last mapping
    /// @return Iterator to the element following the last mapping
    IteratorType end() noexcept { return mapping_.end(); }

    /// @brief Gets an read-only iterator to the beginning of the mappings
    /// @return Iterator to the first mapping
    ConstIteratorType begin() const noexcept { return mapping_.cbegin(); }

    /// @brief Gets a read-only iterator to the element following the last mapping
    /// @return Iterator to the element following the last mapping
    ConstIteratorType end() const noexcept { return mapping_.cend(); }

    /// @brief Gets an read-only iterator to the beginning of the mappings
    /// @return Iterator to the first mapping
    ConstIteratorType cbegin() const noexcept { return mapping_.cbegin(); }

    /// @brief Gets a read-only iterator to the element following the last mapping
    /// @return Iterator to the element following the last mapping
    ConstIteratorType cend() const noexcept { return mapping_.cend(); }

  private:
    std::vector<MappingEntry> mapping_;
};

} // namespace hgps
