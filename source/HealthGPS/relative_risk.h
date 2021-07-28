#pragma once
#include <map>

#include "gender_table.h"

namespace hgps {

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

	class RelativeRisk {
	public:
		RelativeRisk() = delete;
		RelativeRisk(std::map<std::string, FloatAgeGenderTable>&& disease,
			std::map<std::string, std::map<core::Gender, RelativeRiskLookup>>&& risk_factor);

		const std::map<std::string, FloatAgeGenderTable>& disease() const noexcept;

		const std::map<std::string, std::map<core::Gender, RelativeRiskLookup>>& risk_factor() const noexcept;

	private:
		std::map<std::string, FloatAgeGenderTable> disease_;
		std::map<std::string, std::map<core::Gender, RelativeRiskLookup>> risk_factor_;
	};
}

