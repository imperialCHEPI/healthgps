#include <tuple>
#include <algorithm>
#include <iterator>

#include "mapping.h"

namespace hgps {

	MappingEntry::MappingEntry(std::string name, short level, std::string entity_name)
		:name_{ name }, level_{ level }, entity_name_{ entity_name } {}

	MappingEntry::MappingEntry(std::string name, short level)
		: MappingEntry(name, level, "") {}

	std::string MappingEntry::name() const noexcept { return name_; }

	int MappingEntry::level() const noexcept { return level_; }

	std::string MappingEntry::entity_name() const noexcept { return entity_name_; }

	bool MappingEntry::is_entity() const noexcept { return entity_name_.length() > 0; }

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

	std::size_t HierarchicalMapping::size() const noexcept { 
		return mapping_.size(); 
	}

	MappingEntry& HierarchicalMapping::operator[](std::size_t index) {
		return mapping_[index];
	}

	const MappingEntry& HierarchicalMapping::operator[](std::size_t index) const {
		return mapping_[index];
	}

	int HierarchicalMapping::max_level() const noexcept {
		if (mapping_.empty()) {
			return 0;
		}

		return mapping_.back().level();
	}

	std::vector<MappingEntry> HierarchicalMapping::at_level(const int level) const noexcept {
		std::vector<MappingEntry> out;
		std::copy_if(mapping_.begin(), mapping_.end(), std::back_inserter(out),
			[&level](const auto& elem) {return elem.level() == level; });

		return out;
	}
}