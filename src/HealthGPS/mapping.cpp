#include "mapping.h"
#include "HealthGPS.Core/string_util.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <tuple>

namespace hgps {

MappingEntry::MappingEntry(std::string name, int level, core::Identifier entity_key,
                           FactorRange range)
    : name_{name}, name_key_{core::Identifier{name}}, level_{level},
      entity_key_{std::move(entity_key)}, range_{range} {}

MappingEntry::MappingEntry(std::string name, int level, core::Identifier entity_key)
    : MappingEntry(name, level, entity_key, FactorRange{}) {}

MappingEntry::MappingEntry(std::string name, int level)
    : MappingEntry(name, level, core::Identifier::empty(), FactorRange{}) {}

const std::string &MappingEntry::name() const noexcept { return name_; }

int MappingEntry::level() const noexcept { return level_; }

const core::Identifier &MappingEntry::entity_key() const noexcept {
    return is_entity() ? entity_key_ : name_key_;
}

bool MappingEntry::is_entity() const noexcept { return !entity_key_.is_empty(); }

const core::Identifier &MappingEntry::key() const noexcept { return name_key_; }

const FactorRange &MappingEntry::range() const noexcept { return range_; }

double MappingEntry::get_bounded_value(const double &value) const noexcept {
    if (range_.empty) {
        return value;
    }

    return std::min(std::max(value, range_.minimum), range_.maximum);
}

inline bool operator>(const MappingEntry &lhs, const MappingEntry &rhs) {
    return lhs.level() > rhs.level() || ((lhs.level() == rhs.level()) && lhs.name() > rhs.name());
}

inline bool operator<(const MappingEntry &lhs, const MappingEntry &rhs) {
    return lhs.level() < rhs.level() || ((lhs.level() == rhs.level()) && lhs.name() < rhs.name());
}

/*-------------   Hierarchical Mapping implementation ------------- */

HierarchicalMapping::HierarchicalMapping(std::vector<MappingEntry> &&mapping)
    : mapping_{std::move(mapping)} {

    std::sort(mapping_.begin(), mapping_.end());
}

const std::vector<MappingEntry> &HierarchicalMapping::entries() const noexcept { return mapping_; }

std::size_t HierarchicalMapping::size() const noexcept { return mapping_.size(); }

int HierarchicalMapping::max_level() const noexcept {
    if (mapping_.empty()) {
        return 0;
    }

    return mapping_.back().level();
}

MappingEntry HierarchicalMapping::at(const core::Identifier &key) const {
    auto it = std::find_if(mapping_.cbegin(), mapping_.cend(),
                           [&key](const auto &item) { return item.key() == key; });
    if (it != mapping_.end()) {
        return *it;
    }

    throw std::out_of_range(
        "The mapping container does not have an element with the specified key.");
}

std::vector<MappingEntry> HierarchicalMapping::at_level(int level) const noexcept {
    std::vector<MappingEntry> result;
    std::copy_if(mapping_.cbegin(), mapping_.cend(), std::back_inserter(result),
                 [&level](const auto &elem) { return elem.level() == level; });

    return result;
}

} // namespace hgps