#include <tuple>
#include <algorithm>
#include <iterator>

#include "mapping.h"
#include "HealthGPS.Core/string_util.h"
#include <stdexcept>

namespace hgps {
	hgps::MappingEntry::MappingEntry(std::string name, short level,
		std::string entity_name, FactorRange range, bool dynamic_factor)
		: name_{ name }, name_key_{ core::to_lower(name) }, level_{ level }
		, entity_name_{ core::to_lower(entity_name) }, range_{range}
		, dynamic_factor_{ dynamic_factor } { }

	MappingEntry::MappingEntry(std::string name, short level,
		std::string entity_name, bool dynamic_factor)
		: MappingEntry(name, level, entity_name, FactorRange{}, dynamic_factor) {}

	MappingEntry::MappingEntry(std::string name, short level, std::string entity_name, FactorRange range)
		: MappingEntry(name, level, entity_name, range, false) {}

	MappingEntry::MappingEntry(std::string name, short level, std::string entity_name)
		: MappingEntry(name, level, entity_name, FactorRange{}, false) {}

	MappingEntry::MappingEntry(std::string name, short level)
		: MappingEntry(name, level, std::string{}, FactorRange{}, false) {}

	const std::string&  MappingEntry::name() const noexcept { return name_; }

	short MappingEntry::level() const noexcept { return level_; }

	const std::string& MappingEntry::entity_name() const noexcept { return entity_name_; }

	const std::string& MappingEntry::entity_key() const noexcept {
		return is_entity() ? entity_name_ : name_key_;
	}

	bool MappingEntry::is_entity() const noexcept { return entity_name_.length() > 0; }

	bool MappingEntry::is_dynamic_factor() const noexcept { return dynamic_factor_; }

	const std::string& MappingEntry::key() const noexcept { return name_key_; }

	const FactorRange& MappingEntry::range() const noexcept {
		return range_;
	}

	double MappingEntry::get_bounded_value(const double& value) const noexcept {
		if (range_.empty) {
			return value;
		}

		return std::min(std::max(value, range_.minimum), range_.maximum);
	}

	inline bool operator> (const MappingEntry& lhs, const MappingEntry& rhs) {
		return lhs.level() > rhs.level() || 
			 ((lhs.level() == rhs.level()) && lhs.name() > rhs.name());
	}

	inline bool operator< (const MappingEntry& lhs, const MappingEntry& rhs) {
		return lhs.level() < rhs.level() ||
			((lhs.level() == rhs.level()) && lhs.name() < rhs.name());
	}

	/*-------------   Hierarchical Mapping implementation ------------- */


	HierarchicalMapping::HierarchicalMapping(std::vector<MappingEntry>&& mapping)
		: mapping_{ std::move(mapping) } {

		std::sort(mapping_.begin(), mapping_.end());
	}

	const std::vector<MappingEntry>& HierarchicalMapping::entries() const noexcept {
		return mapping_;
	}

	std::vector<MappingEntry> HierarchicalMapping::entries_without_dynamic() const noexcept {
		std::vector<MappingEntry> result;
		std::copy_if(mapping_.begin(), mapping_.end(), std::back_inserter(result),
			[](const auto& elem) {return !elem.is_dynamic_factor(); });

		return result;
	}

	std::size_t HierarchicalMapping::size() const noexcept { 
		return mapping_.size(); 
	}

	int HierarchicalMapping::max_level() const noexcept {
		if (mapping_.empty()) {
			return 0;
		}

		return mapping_.back().level();
	}

	MappingEntry HierarchicalMapping::at(const std::string& name) const {
		auto other_key = core::to_lower(name);
		auto it = std::find_if(mapping_.begin(), mapping_.end(),
			[&other_key](const auto& item) { return item.key() == other_key; });
		if (it != mapping_.end()) {
			return *it;
		}

		throw std::out_of_range(
			"The mapping container does not have an element with the specified key.");
	}

	std::vector<MappingEntry> HierarchicalMapping::at_level(int level) const noexcept {
		std::vector<MappingEntry> result;
		std::copy_if(mapping_.begin(), mapping_.end(), std::back_inserter(result),
			[&level](const auto& elem) {return elem.level() == level; });

		return result;
	}

	std::vector<MappingEntry> HierarchicalMapping::at_level_without_dynamic(int level) const noexcept {
		std::vector<MappingEntry> result;
		std::copy_if(mapping_.begin(), mapping_.end(), std::back_inserter(result),
			[&level](const auto& elem) {
				return elem.level() == level && !elem.is_dynamic_factor();
			});

		return result;
	}
}