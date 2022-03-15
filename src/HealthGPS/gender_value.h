#pragma once
#include <type_traits>

namespace hgps {

	template<typename T> requires std::is_arithmetic_v<T>
	struct GenderValue {
		GenderValue() = default;
		GenderValue(T males_value, T females_value)
			: males{ males_value }, females{ females_value }
		{}

		T males;
		T females;

		T total() const noexcept { return males + females; }
	};

	using IntegerGenderValue = GenderValue<int>;
	using FloatGenderValue = GenderValue<float>;
	using DoubleGenderValue = GenderValue<double>;
}
