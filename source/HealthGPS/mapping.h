#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace hgps {

	struct FactorRange {
		FactorRange() = default;
		FactorRange(const double min_value, const double max_value)
			: empty{ false }, minimum {min_value}, maximum{max_value} {
			if (min_value > max_value) {
				throw std::invalid_argument("Factor range minimum must not be greater than maximum.");
			}
		}

		bool empty{ true };
		double minimum{};
		double maximum{};
	};

	class MappingEntry {
	public:
		MappingEntry() = delete;
		MappingEntry(std::string name, const short level, std::string entity_name,
			FactorRange range, const bool dynamic_factor);

		MappingEntry(std::string name, const short level, std::string entity_name, const bool dynamic_factor);
		MappingEntry(std::string name, const short level, std::string entity_name, FactorRange range);
		MappingEntry(std::string name, const short level, std::string entity_name);
		MappingEntry(std::string name, const short level);

		std::string name() const noexcept;

		short level() const noexcept;

		std::string entity_name() const noexcept;

		std::string entity_key() const noexcept;

		bool is_entity() const noexcept;

		bool is_dynamic_factor() const noexcept;

		std::string key() const noexcept;

		const FactorRange& range() const noexcept;

		double get_bounded_value(const double& value) const noexcept;

	private:
		std::string name_;
		std::string name_key_;
		short level_{};
		std::string entity_name_;
		FactorRange range_;
		bool dynamic_factor_;
	};

	class HierarchicalMapping {
	public:
		using IteratorType = std::vector<MappingEntry>::iterator;
		using ConstIteratorType = std::vector<MappingEntry>::const_iterator;

		HierarchicalMapping() = delete;
		HierarchicalMapping(std::vector<MappingEntry>&& mapping);

		const std::vector<MappingEntry>& entries() const noexcept;

		std::vector<MappingEntry> entries_without_dynamic() const noexcept;

		std::size_t size() const noexcept;

		int max_level() const noexcept;

		MappingEntry at(const std::string name) const;

		std::vector<MappingEntry> at_level(const int level) const noexcept;

		std::vector<MappingEntry> at_level_without_dynamic(const int level) const noexcept;

		IteratorType begin() noexcept { return mapping_.begin(); }
		IteratorType end() noexcept { return mapping_.end(); }

		ConstIteratorType begin() const noexcept { return mapping_.begin(); }
		ConstIteratorType end() const noexcept { return mapping_.end(); }

	private:
		std::vector<MappingEntry> mapping_;
	};
}