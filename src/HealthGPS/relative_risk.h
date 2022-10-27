#pragma once
#include "gender_table.h"
#include "monotonic_vector.h"
#include "HealthGPS.Core/array2d.h"
#include "HealthGPS.Core/identifier.h"
#include <map>

namespace hgps {

	class RelativeRiskLookup {
	public:
		RelativeRiskLookup() = delete;

		RelativeRiskLookup(
			const MonotonicVector<int>& rows,
			const MonotonicVector<float>& cols,
			core::FloatArray2D&& values);

		std::size_t size() const noexcept;

		std::size_t rows() const noexcept;

		std::size_t columns() const noexcept;

		bool empty() const noexcept;

		float at(const int age, const float value) const;

		float operator()(const int age, const float value) const;

		bool contains(const int age, const float value) const noexcept;

	private:
		core::FloatArray2D table_;
		std::map<int, int> rows_index_;
		std::map<float, int> cols_index_;

		float lookup_value(const int age, const float value) const noexcept;
	};

	using RelativeRiskTableMap = std::map<core::Identifier, FloatAgeGenderTable>;
	using RelativeRiskLookupMap = std::map<core::Identifier, std::map<core::Gender, RelativeRiskLookup>>;

	struct RelativeRisk
	{
		RelativeRiskTableMap diseases;
		RelativeRiskLookupMap risk_factors;
	};
}

