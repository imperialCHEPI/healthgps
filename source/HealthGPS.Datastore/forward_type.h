#pragma once
#include <type_traits>

// forward type declaration
namespace hgps {
	namespace data {

		// C++20 concept for numeric columns types
		template <typename T>
		concept Numerical = std::is_arithmetic_v<T>;

		class DataTable;
		class DataTableColumn;

		template <typename ColumnType>
		class DataTableColumnIterator;

		template<Numerical TYPE>
		class NumericDataTableColumn;

		class StringDataTableColumn;
		class FloatDataTableColumn;
		class DoubleDataTableColumn;
		class IntegerDataTableColumn;

		class DataTableColumnVisitor;
	}
}