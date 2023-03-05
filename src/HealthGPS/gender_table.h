#pragma once
#include <map>
#include <numeric>

#include "monotonic_vector.h"
#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/array2d.h"
#include "HealthGPS.Core/interval.h"

namespace hgps {

	/// @brief Defines the gender column lookup table data type
	/// @tparam ROW The rows value type
	/// @tparam TYPE The cell value type
	template<core::Numerical ROW, core::Numerical TYPE>
	class GenderTable {
	public:
		/// @brief Initialises a new instance of the GenderTable class
		GenderTable() = default;

		/// @brief Initialises a new instance of the GenderTable class
		/// @param rows The monotonic rows lookup breakpoints
		/// @param cols The gender columns lookup breakpoints
		/// @param values The full lookup-table values
		/// @throws std::invalid_argument for lookup breakpoints and values table size mismatch
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

		/// @brief Gets the lookup table size
		/// @return Lookup table size
		std::size_t size() const noexcept {
			return table_.size();
		}

		/// @brief Get the number of rows lookup breakpoints
		/// @return Number of rows
		std::size_t rows() const noexcept {
			return table_.rows();
		}

		/// @brief Get the number of columns lookup breakpoints
		/// @return Number of columns
		std::size_t columns() const noexcept {
			return table_.columns();
		}

		/// @brief Determine whether the lookup table is empty
		/// @return true, if the lookup data is empty; otherwise, false
		bool empty() const noexcept {
			return rows_index_.empty() || cols_index_.empty();
		}

		/// @brief Gets a value at a given row and column intersection
		/// @param row The reference row
		/// @param gender The reference column
		/// @return The lookup value
		/// @throws std::out_of_range for accessing unknown lookup breakpoints
		TYPE& at(const ROW row, const core::Gender gender) {
			return table_(rows_index_.at(row), cols_index_.at(gender));
		}

		/// @brief Gets a read-only value at a given row and column intersection
		/// @param row The reference row
		/// @param gender The reference column
		/// @return The lookup value
		/// @throws std::out_of_range for accessing unknown lookup breakpoints
		const TYPE& at(const ROW row, const core::Gender gender) const {
			return table_(rows_index_.at(row), cols_index_.at(gender));
		}

		/// @brief Gets a value at a given row and column intersection
		/// @param row The reference row
		/// @param gender The reference column
		/// @return The lookup value
		/// @throws std::out_of_range for accessing unknown lookup breakpoints
		TYPE& operator()(const ROW row, const core::Gender gender) {
			return table_(rows_index_.at(row), cols_index_.at(gender));
		}

		/// @brief Gets a read-only value at a given row and column intersection
		/// @param row The reference row
		/// @param gender The reference column
		/// @return The lookup value
		/// @throws std::out_of_range for accessing unknown lookup breakpoints
		const TYPE& operator()(const ROW row, const core::Gender gender) const {
			return table_(rows_index_.at(row), cols_index_.at(gender));
		}

		/// @brief Determines whether the lookup contains a row
		/// @param row The row breakpoint value 
		/// @return true, if the lookup contains the row; otherwise, false
		bool contains(const ROW row) const noexcept {
			return rows_index_.contains(row);
		}

		/// @brief Determines whether the lookup contains a value
		/// @param row The row breakpoint value 
		/// @param gender The column breakpoint value
		/// @return true, if the lookup contains the value; otherwise, false
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

	/// @brief Defines the age and gender lookup table data type
	/// @tparam TYPE The cell value type
	template<core::Numerical TYPE>
	class AgeGenderTable : public GenderTable<int,TYPE> {
	public:
		/// @brief Initialises a new instance of the AgeGenderTable class
		AgeGenderTable() = default;

		/// @brief Initialises a new instance of the AgeGenderTable class
		/// @param rows The monotonic rows lookup breakpoints
		/// @param cols The gender columns lookup breakpoints
		/// @param values The full lookup-table values
		/// @throws std::invalid_argument for lookup breakpoints and values table size mismatch
		AgeGenderTable(const MonotonicVector<int>& rows,
			const std::vector<core::Gender>& cols, core::Array2D<TYPE>&& values)
			: GenderTable<int, TYPE>(rows, cols, std::move(values)) {}
	};

	/// @brief Creates an instance of the gender lookup table for integer rows breakpoints value
	/// @tparam TYPE The cell value type
	/// @param rows_range The rows breakpoints range
	/// @return A new instance of the GenderTable class
	/// @throws std::out_of_range for rows range 'lower' of negative value or less than the 'upper' value
	template<core::Numerical TYPE>
	GenderTable<int,TYPE> create_integer_gender_table(const core::IntegerInterval& rows_range) {
		if (rows_range.lower() < 0 || rows_range.lower() >= rows_range.upper()) {
			throw std::out_of_range(
				"The 'range lower' value must be greater than zero and less than the 'range upper' value.");
		}

		auto rows = std::vector<int>(static_cast<size_t>(rows_range.length()) + 1);
		std::iota(rows.begin(), rows.end(), rows_range.lower());

		auto cols = std::vector<core::Gender>{ core::Gender::male, core::Gender::female };
		auto data = core::Array2D<TYPE>(rows.size(), cols.size());
		return GenderTable<int, TYPE>(MonotonicVector(rows), cols, std::move(data));
	}

	/// @brief Creates an instance of the age and gender lookup table
	/// @tparam TYPE The values data type
	/// @param age_range The age breakpoints range
	/// @return A new instance of the AgeGenderTable class
	/// @throws std::out_of_range for age range 'lower' of negative value or less than the 'upper' value
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

	/// @brief Age and Gender lookup table for integer values
	using IntegerAgeGenderTable = AgeGenderTable<int>;

	/// @brief Age and Gender lookup table for single precision floating-point values
	using FloatAgeGenderTable = AgeGenderTable<float>;

	/// @brief Age and Gender lookup table for double precision floating-point values
	using DoubleAgeGenderTable = AgeGenderTable<double>;
}