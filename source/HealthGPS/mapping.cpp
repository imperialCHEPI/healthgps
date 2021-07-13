#include <tuple>
#include <algorithm>
#include <iterator>

#include "mapping.h"
#include "HealthGPS.Core/string_util.h"
#include <stdexcept>

namespace hgps {
	hgps::MappingEntry::MappingEntry(std::string name, const short level, 
		std::string entity_name, const bool dynamic_factor) 
		: name_{ name }, level_{ level }, entity_name_{ core::to_lower(entity_name) },
		dynamic_factor_{ dynamic_factor }, name_key_{ core::to_lower(name) } { }

	MappingEntry::MappingEntry(std::string name, const short level, std::string entity_name)
		: MappingEntry(name, level, entity_name, false) {}

	MappingEntry::MappingEntry(std::string name, const short level)
		: MappingEntry(name, level, std::string{}, false) {}

	std::string MappingEntry::name() const noexcept { return name_; }

	short MappingEntry::level() const noexcept { return level_; }

	std::string MappingEntry::entity_name() const noexcept { return entity_name_; }

	std::string MappingEntry::entity_key() const noexcept {
		return is_entity() ? entity_name_ : name_key_;
	}

	bool MappingEntry::is_entity() const noexcept { return entity_name_.length() > 0; }

	bool MappingEntry::is_dynamic_factor() const noexcept { return dynamic_factor_; }

	std::string MappingEntry::key() const noexcept { return name_key_; }

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
		: mapping_{ mapping } {

		std::sort(mapping_.begin(), mapping_.end());
	}

	const std::vector<MappingEntry>& HierarchicalMapping::entries() const noexcept {
		return mapping_;
	}

	std::vector<MappingEntry> HierarchicalMapping::entries_without_dynamic() const noexcept {
		std::vector<MappingEntry> out;
		std::copy_if(mapping_.begin(), mapping_.end(), std::back_inserter(out),
			[](const auto& elem) {return !elem.is_dynamic_factor(); });

		return out;
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

	MappingEntry HierarchicalMapping::at(const std::string name) const {
		auto it = std::find_if(mapping_.begin(), mapping_.end(),
			[&name](const auto& item) {return item.key() == core::to_lower(name); });
		if (it != mapping_.end()) {
			return *it;
		}

		throw std::out_of_range(
			"The mapping container does not have an element with the specified key.");
	}

	std::vector<MappingEntry> HierarchicalMapping::at_level(const int level) const noexcept {
		std::vector<MappingEntry> out;
		std::copy_if(mapping_.begin(), mapping_.end(), std::back_inserter(out),
			[&level](const auto& elem) {return elem.level() == level; });

		return out;
	}

	std::vector<MappingEntry> HierarchicalMapping::at_level_without_dynamic(const int level) const noexcept {
		std::vector<MappingEntry> out;
		std::copy_if(mapping_.begin(), mapping_.end(), std::back_inserter(out),
			[&level](const auto& elem) {
				return elem.level() == level && !elem.is_dynamic_factor();
			});

		return out;
	}
}