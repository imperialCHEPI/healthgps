#pragma once
#include "gender_table.h"
#include "monotonic_vector.h"
#include "HealthGPS.Core/array2d.h"
#include "HealthGPS.Core/identifier.h"
#include <map>

namespace hgps {

	/// @brief Defines the relative risk factors lookup data type
	///
	/// @note When a lookup value does not exists at the exact
	/// breakpoint values provided, a data at bound value or a 
	/// linear interpolated value is returned for the operation.
	class RelativeRiskLookup {
	public:
		RelativeRiskLookup() = delete;

		/// @brief Initialises a new instance of the RelativeRiskLookup class.
		/// @param rows The rows lookup breakpoints
		/// @param cols The columns lookup breakpoints
		/// @param values The lookup data values
		/// @throws std::out_of_range for lookup breakpoints and values size mismatch.
		RelativeRiskLookup(
			const MonotonicVector<int>& rows,
			const MonotonicVector<float>& cols,
			core::FloatArray2D&& values);

		/// @brief Gets the number of elements in the lookup dataset
		/// @return Number of lookup elements.
		std::size_t size() const noexcept;

		/// @brief Gets the number of rows lookup breakpoints
		/// @return Number of rows
		std::size_t rows() const noexcept;

		/// @brief Gets the number of columns lookup breakpoints
		/// @return Number of columns
		std::size_t columns() const noexcept;

		/// @brief Checks if the lookup has no data.
		/// @return true if the lookup data is empty; otherwise, false.
		bool empty() const noexcept;

		/// @brief Lookup a value at specific breakpoints with bounds checking
		/// @param age The row value
		/// @param value The column value
		/// @return The lookup value
		/// @throws std::out_of_range for row breakpoint value outside of the bounds.
		float at(const int age, const float value) const;

		/// @brief Lookup a value at specific breakpoints with bounds checking
		/// @param age The row value
		/// @param value The column value
		/// @return The lookup value
		/// @throws std::out_of_range for row breakpoint value outside of the bounds.
		float operator()(const int age, const float value) const;

		/// @brief Checks if the lookup contains a value with specific breakpoints
		/// @param age The row value
		/// @param value The column value
		/// @return true if there is such a value; otherwise, false.
		bool contains(const int age, const float value) const noexcept;

	private:
		core::FloatArray2D table_;
		std::map<int, int> rows_index_;
		std::map<float, int> cols_index_;

		float lookup_value(const int age, const float value) const noexcept;
	};

	/// @brief Defines the relative risk factor table type
	using RelativeRiskTableMap = std::map<core::Identifier, FloatAgeGenderTable>;

	/// @brief Defines the relative risk factor lookup type
	using RelativeRiskLookupMap = std::map<core::Identifier, std::map<core::Gender, RelativeRiskLookup>>;

	/// @brief Defines the relative risk factors data type
	struct RelativeRisk
	{
		/// @brief Disease to diseases relative risk values
		RelativeRiskTableMap diseases;

		/// @brief Risk factor to diseases relative risk values
		RelativeRiskLookupMap risk_factors;
	};
}

