#pragma once
#include "HealthGPS.Core/identifier.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

namespace hgps {

	inline const core::Identifier InterceptKey = core::Identifier{ "intercept" };

	struct FactorRange {
		FactorRange() = default;
		FactorRange(double min_value, double max_value)
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
		MappingEntry(std::string name, short level, core::Identifier entity_key,
			FactorRange range, bool dynamic_factor);

		MappingEntry(std::string name, short level, core::Identifier entity_key, bool dynamic_factor);
		MappingEntry(std::string name, short level, core::Identifier entity_key, FactorRange range);
		MappingEntry(std::string name, short level, core::Identifier entity_key);
		MappingEntry(std::string name, short level);

		const std::string& name() const noexcept;

		short level() const noexcept;

		const core::Identifier& key() const noexcept;

		const core::Identifier& entity_key() const noexcept;

		bool is_entity() const noexcept;

		bool is_dynamic_factor() const noexcept;

		const FactorRange& range() const noexcept;

		double get_bounded_value(const double& value) const noexcept;

	private:
		std::string name_;
		core::Identifier name_key_;
		short level_{};
		core::Identifier entity_key_;
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

		MappingEntry at(const core::Identifier& key) const;

		std::vector<MappingEntry> at_level(int level) const noexcept;

		std::vector<MappingEntry> at_level_without_dynamic(int level) const noexcept;

		IteratorType begin() noexcept { return mapping_.begin(); }
		IteratorType end() noexcept { return mapping_.end(); }

		ConstIteratorType begin() const noexcept { return mapping_.begin(); }
		ConstIteratorType end() const noexcept { return mapping_.end(); }

		ConstIteratorType cbegin() const noexcept { return mapping_.cbegin(); }
		ConstIteratorType cend() const noexcept { return mapping_.cend(); }

	private:
		std::vector<MappingEntry> mapping_;
	};
}