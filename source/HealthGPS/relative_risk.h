#pragma once
#include <map>

#include "gender_table.h"
#include "monotonic_vector.h"
#include "HealthGPS.Core\array2d.h"

namespace hgps {

	class RelativeRiskLookup;
	
	using RelativeRiskTableMap = std::map<std::string, FloatAgeGenderTable>;
	using RelativeRiskLookupMap = std::map<std::string, std::map<core::Gender, RelativeRiskLookup>>;

	struct RelativeRisk
	{
		RelativeRiskTableMap diseases;
		RelativeRiskLookupMap risk_factors;
	};

	class RelativeRiskLookup {
	public:
		RelativeRiskLookup() = delete;

		RelativeRiskLookup(
			MonotonicVector<int> rows,
			MonotonicVector<float> cols,
			core::FloatArray2D&& values);

		std::size_t size() const noexcept;

		std::size_t rows() const noexcept;

		std::size_t columns() const noexcept;

		bool empty() const noexcept;

		float at(const int age, const float value) const;

		float operator()(const int age, const float value);

		const float operator()(const int age, const float value) const;

		bool contains(const int age, const float value) const noexcept;

	private:
		core::FloatArray2D table_;
		std::map<int, int> rows_index_;
		std::map<float, int> cols_index_;

		float lookup_value(const int age, const float value) const noexcept;
	};
}

