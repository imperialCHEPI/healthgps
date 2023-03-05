#pragma once
#include <type_traits>

namespace hgps {

	/// @brief Defines a gender associated value data type
	/// @tparam T The value type
	template<typename T> requires std::is_arithmetic_v<T>
	struct GenderValue {
		/// @brief Initialises a new instance of the GenderValue structure
		GenderValue() = default;

		/// @brief Initialises a new instance of the GenderValue structure
		/// @param males_value The males value
		/// @param females_value The female value
		GenderValue(T males_value, T females_value)
			: males{ males_value }, females{ females_value }
		{}

		/// @brief Males value
		T males;

		/// @brief Females value
		T females;

		/// @brief Gets the total value for males and females
		/// @return Total value
		T total() const noexcept { return males + females; }
	};

	/// @brief Gender value for integer value
	using IntegerGenderValue = GenderValue<int>;

	/// @brief Gender value for single precision floating-point value
	using FloatGenderValue = GenderValue<float>;

	/// @brief Gender value for double precision floating-point value
	using DoubleGenderValue = GenderValue<double>;
}
