#pragma once
#include "HealthGPS.Core/identifier.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace hgps {

	/// @brief The constant in the regression model presentation identifier 
	inline const core::Identifier InterceptKey = core::Identifier{ "intercept" };

	/// @brief Defines the risk factor allowed range data type
	///
	/// @details The factors range is defined from the fitted dataset and enforced
	/// by the simulation algorithm. The default constructor, creates an empty range.
	struct FactorRange {
		/// @brief Initialises a new instance of the FactorRange structure
		FactorRange() = default;

		/// @brief Initialises a new instance of the FactorRange structure
		/// @param min_value Minimum factor value
		/// @param max_value Maximum factor value
		/// @throws std::invalid_argument for minimum greater than the maximum value
		FactorRange(double min_value, double max_value)
			: empty{ false }, minimum {min_value}, maximum{max_value} {
			if (min_value > max_value) {
				throw std::invalid_argument("Factor range minimum must not be greater than maximum.");
			}
		}

		/// @brief Gets a value indicating whether the range is empty, no limits.
		bool empty{ true };

		/// @brief The range minimum value
		double minimum{};

		/// @brief The range maximum value
		double maximum{};
	};

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
		/// @param entity_key The associated Person property, if exists
		/// @param range The factor range
		/// @param dynamic_factor indicates whether this is the dynamic factor
		MappingEntry(std::string name, short level, core::Identifier entity_key,
			FactorRange range, bool dynamic_factor);

		/// @brief Initialises a new instance of the MappingEntry class
		/// @param name Risk factor name
		/// @param level The hierarchical level
		/// @param entity_key The associated Person property, if exists
		/// @param dynamic_factor indicates whether this is the dynamic factor
		MappingEntry(std::string name, short level, core::Identifier entity_key, bool dynamic_factor);

		/// @brief Initialises a new instance of the MappingEntry class
		/// @param name Risk factor name
		/// @param level The hierarchical level
		/// @param entity_key The associated Person property, if exists
		/// @param range The factor range
		MappingEntry(std::string name, short level, core::Identifier entity_key, FactorRange range);
		
		/// @brief Initialises a new instance of the MappingEntry class
		/// @param name Risk factor name
		/// @param level The hierarchical level
		/// @param entity_key The associated Person property, if exists
		MappingEntry(std::string name, short level, core::Identifier entity_key);

		/// @brief Initialises a new instance of the MappingEntry class
		/// @param name Risk factor name
		/// @param level The hierarchical level
		MappingEntry(std::string name, short level);

		/// @brief Gets the factor name
		/// @return Factor name
		const std::string& name() const noexcept;

		/// @brief Gets the factor hierarchical level
		/// @return Factor level
		short level() const noexcept;

		/// @brief Gets the factor unique identification
		/// @return Factor identification
		const core::Identifier& key() const noexcept;

		/// @brief Gets the factor's associated Person property identifier
		/// @return Associated entry identifier
		const core::Identifier& entity_key() const noexcept;

		/// @brief Determine whether this factor has an associated Person property, e.g. age
		/// @return true, if the factor has an associated property; otherwise, false.
		bool is_entity() const noexcept;

		/// @brief Determine whether this instance is a dynamic factor
		/// @return true, for the dynamic factor; otherwise, false.
		bool is_dynamic_factor() const noexcept;

		/// @brief Gets the factor allowed values range
		/// @return Factor values range
		const FactorRange& range() const noexcept;

		/// @brief Adjusts a value to the factor range, if provided
		/// @param value The value to adjust
		/// @return The bounded value
		double get_bounded_value(const double& value) const noexcept;

	private:
		std::string name_;
		core::Identifier name_key_;
		short level_{};
		core::Identifier entity_key_;
		FactorRange range_;
		bool dynamic_factor_;
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
		HierarchicalMapping(std::vector<MappingEntry>&& mapping);

		/// @brief Gets the collection of mapping entries
		/// @return The mapping entries
		const std::vector<MappingEntry>& entries() const noexcept;

		/// @brief Gets the collection of mapping entries without the dynamic factor
		/// @return The mapping entries without dynamic factor
		std::vector<MappingEntry> entries_without_dynamic() const noexcept;
		
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
		MappingEntry at(const core::Identifier& key) const;

		/// @brief Gets the mapping entries at a given hierarchical level
		/// @param level The hierarchical level
		/// @return The collection of mappings
		std::vector<MappingEntry> at_level(int level) const noexcept;

		/// @brief Gets the mapping entries at a given hierarchical level without the dynamic factor
		/// @param level The hierarchical level
		/// @return The collection of mappings without the dynamic factor
		std::vector<MappingEntry> at_level_without_dynamic(int level) const noexcept;

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
}