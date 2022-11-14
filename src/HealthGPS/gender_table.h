#pragma once
#include <map>
#include <numeric>

#include "monotonic_vector.h"
#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/array2d.h"
#include "HealthGPS.Core/interval.h"

namespace hgps {
	template<core::Numerical ROW, core::Numerical TYPE>
	class GenderTable {
	public:
		GenderTable() = default;
		GenderTable(const MonotonicVector<ROW>& rows,
			const std::vector<core::Gender>& cols, core::Array2D<TYPE>&& values)
			: table_{ std::move(values) }, rows_index_{}, cols_index_{}
		{
			if (rows.size() != table_.rows() || cols.size() != table_.columns()) {
				throw std::invalid_argument("Lookup breakpoints and values size mismatch.");
			}

			auto rows_count = static_cast<int>(rows.size());
			for (auto index = 0; index < rows_count; index++) {
				rows_index_.emplace(rows[index], index);
			}

			auto cols_count = static_cast<int>(cols.size());
			for (auto index = 0; index < cols_count; index++) {
				cols_index_.emplace(cols[index], index);
			}
		}

		std::size_t size() const noexcept {
			return table_.size();
		}

		std::size_t rows() const noexcept {
			return table_.rows();
		}

		std::size_t columns() const noexcept {
			return table_.columns();
		}

		bool empty() const noexcept {
			return rows_index_.empty() || cols_index_.empty();
		}

		TYPE& at(const ROW row, const core::Gender gender) {
			return table_(rows_index_.at(row), cols_index_.at(gender));
		}

		const TYPE& at(const ROW row, const core::Gender gender) const {
			return table_(rows_index_.at(row), cols_index_.at(gender));
		}

		TYPE& operator()(const ROW row, const core::Gender gender) {
			return table_(rows_index_.at(row), cols_index_.at(gender));
		}

		const TYPE& operator()(const ROW row, const core::Gender gender) const {
			return table_(rows_index_.at(row), cols_index_.at(gender));
		}

		bool contains(const ROW row) const noexcept {
			return rows_index_.contains(row);
		}

		bool contains(const ROW row, const core::Gender gender) const noexcept {
			if (rows_index_.contains(row)) {
				return cols_index_.contains(gender);
			}

			return false;
		}

	private:
		core::Array2D<TYPE> table_{};
		std::map<ROW, int> rows_index_{};
		std::map<core::Gender, int> cols_index_{};
	};

	template<core::Numerical TYPE>
	class AgeGenderTable : public GenderTable<int,TYPE> {
	public:
		AgeGenderTable() = default;
		AgeGenderTable(const MonotonicVector<int>& rows,
			const std::vector<core::Gender>& cols, core::Array2D<TYPE>&& values)
			: GenderTable<int, TYPE>(rows, cols, std::move(values)) {}
	};

	template<core::Numerical TYPE>
	GenderTable<int,TYPE> create_integer_gender_table(const core::IntegerInterval& rows_range) {
		if (rows_range.lower() < 0 || rows_range.lower() >= rows_range.upper()) {
			throw std::invalid_argument(
				"The 'range lower' value must be greater than zero and less than the 'range upper' value.");
		}

		int offset = 1;
		auto rows = std::vector<int>(rows_range.length() + offset);
		std::iota(rows.begin(), rows.end(), rows_range.lower());

		auto cols = std::vector<core::Gender>{ core::Gender::male, core::Gender::female };
		auto data = core::Array2D<TYPE>(rows.size(), cols.size());
		return GenderTable<int, TYPE>(MonotonicVector(rows), cols, std::move(data));
	}

	template<core::Numerical TYPE>
	AgeGenderTable<TYPE> create_age_gender_table(const core::IntegerInterval& age_range) {
		if (age_range.lower() < 0 || age_range.lower() >= age_range.upper()) {
			throw std::invalid_argument(
				"The 'age lower' value must be greater than zero and less than the 'age upper' value.");
		}

		auto rows = std::vector<int>(static_cast<std::size_t>(age_range.length()) + 1);
		std::iota(rows.begin(), rows.end(), age_range.lower());

		auto cols = std::vector<core::Gender>{ core::Gender::male, core::Gender::female };
		auto data = core::Array2D<TYPE>(rows.size(), cols.size());
		return AgeGenderTable<TYPE>(MonotonicVector(rows), cols, std::move(data));
	}

	using IntegerAgeGenderTable = AgeGenderTable<int>;
	using FloatAgeGenderTable = AgeGenderTable<float>;
	using DoubleAgeGenderTable = AgeGenderTable<double>;
}