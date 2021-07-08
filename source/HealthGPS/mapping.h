#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace hgps {

	class MappingEntry {
	public:
		MappingEntry() = delete;
		MappingEntry(std::string name, short level, std::string entity_name);
		MappingEntry(std::string name, short level);

		std::string name() const noexcept;

		int level() const noexcept;

		std::string entity_name() const noexcept;

		bool is_entity() const noexcept;

	private:
		std::string name_;
		short level_{};
		std::string entity_name_;
	};

	class HierarchicalMapping {
	public:
		using IteratorType = std::vector<MappingEntry>::iterator;
		using ConstIteratorType = std::vector<MappingEntry>::const_iterator;

		HierarchicalMapping() = delete;
		HierarchicalMapping(std::vector<MappingEntry>&& mapping);

		const std::vector<MappingEntry>& entries() const noexcept;

		std::size_t size() const noexcept;

		int max_level() const noexcept;

		std::vector<MappingEntry> at_level(const int level) const noexcept;

		MappingEntry& operator[](std::size_t index);

		const MappingEntry& operator[](std::size_t index) const;

		IteratorType begin() noexcept { return mapping_.begin(); }
		IteratorType end() noexcept { return mapping_.end(); }

		ConstIteratorType begin() const noexcept { return mapping_.begin(); }
		ConstIteratorType end() const noexcept { return mapping_.end(); }

	private:
		std::vector<MappingEntry> mapping_;
	};
}