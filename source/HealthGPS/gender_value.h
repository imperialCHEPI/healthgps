#pragma once
namespace hgps {

	template<typename T>
	struct GenderValue {
		GenderValue() = default;
		GenderValue(T male_value, T female_value)
			: male{ male_value }, female{ female_value }
		{}

		T male;
		T female;
	};

	using IntegerGenderValue = GenderValue<int>;
	using FloatGenderValue = GenderValue<float>;
	using DoubleGenderValue = GenderValue<double>;
}
