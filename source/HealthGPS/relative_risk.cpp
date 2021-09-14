#include "relative_risk.h"

namespace hgps {

	RelativeRiskLookup::RelativeRiskLookup(
		const MonotonicVector<int> rows, const MonotonicVector<float> cols, core::FloatArray2D&& values)
		: rows_index_{}, cols_index_{}, table_{ values }
	{
		if (rows.size() != values.rows() || cols.size() != values.columns()) {
			throw std::invalid_argument("Lookup breakpoints and values size mismatch.");
		}

		for (int index = 0; index < rows.size(); index++) {
			rows_index_.emplace(rows[index], index);
		}

		for (int index = 0; index < cols.size(); index++) {
			cols_index_.emplace(cols[index], index);
		}
	}

	std::size_t RelativeRiskLookup::size() const noexcept {
		return table_.size();
	}

	std::size_t RelativeRiskLookup::rows() const noexcept {
		return table_.rows();
	}

	std::size_t RelativeRiskLookup::columns() const noexcept {
		return table_.columns();
	}

	bool RelativeRiskLookup::empty() const noexcept {
		return rows_index_.empty() || cols_index_.empty();
	}

	float RelativeRiskLookup::at(const int age, const float value) const {
		return lookup_value(age, value);
	}

	float RelativeRiskLookup::operator()(const int age, const float value) {
		return lookup_value(age, value);
	}

	const float RelativeRiskLookup::operator()(const int age, const float value) const {
		return lookup_value(age, value);
	}

	bool RelativeRiskLookup::contains(const int age, const float value) const noexcept {
		if (rows_index_.contains(age)) {
			return cols_index_.contains(value);
		}

		return false;
	}

	float RelativeRiskLookup::lookup_value(const int age, const float value) const noexcept {
		if (empty()) {
			return std::nanf("");
		}

		auto row_index = rows_index_.at(age);
		if (value <= cols_index_.begin()->first) {
			return table_(row_index, cols_index_.begin()->second);
		}

		if (value >= cols_index_.rbegin()->first) {
			return table_(row_index, cols_index_.rbegin()->second);
		}

		if (cols_index_.contains(value)) {
			return table_(row_index, cols_index_.at(value));
		}

		auto it = cols_index_.lower_bound(value);
		auto it_prev = std::prev(it);
		auto x1 = it_prev->first;
		auto x2 = it->first;
		auto y1 = table_(row_index, it_prev->second);
		auto y2 = table_(row_index, it->second);

		// Linear interpolation
		return (y2 - y1) * (value - x1) / (x2 - x1) + y1;
	}
}
