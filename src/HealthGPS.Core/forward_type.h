#pragma once
#include <type_traits>
#include <cstdint>

// forward type declaration
namespace hgps {
	namespace core {

		/// @brief Data manager verbosity mode enumeration
		enum class VerboseMode : uint8_t
		{
			/// @brief only report errors
			none,

			/// @brief Print more information about actions, including warning
			verbose
		};

		/// @brief enumerates the gender
		enum class Gender : uint8_t {
			unknown,
			male,
			female
		};

		/// @brief enumerates supported diseases types
		enum class DiseaseGroup : uint8_t {
			other,
			cancer
		};

		// C++20 concept for numeric columns types
		template <typename T>
		concept Numerical = std::is_arithmetic_v<T>;

		/// @brief Lookup table entry for gender values
		struct LookupGenderValue {
			int value{};
			double male{};
			double female{};
		};

		class DataTable;
		class DataTableColumn;

		class StringDataTableColumn;
		class FloatDataTableColumn;
		class DoubleDataTableColumn;
		class IntegerDataTableColumn;

		class DataTableColumnVisitor;
	}
}