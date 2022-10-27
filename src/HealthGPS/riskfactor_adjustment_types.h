#pragma once
#include "HealthGPS.Core/forward_type.h"
#include "HealthGPS.Core/identifier.h"

#include "map2d.h"
#include <vector>

namespace hgps {

	using FactorAdjustmentTable = Map2d<core::Gender, core::Identifier, std::vector<double>>;

	struct BaselineAdjustment final {
		BaselineAdjustment() = default;
		BaselineAdjustment(FactorAdjustmentTable&& adjustment_table)
			: values{ std::move(adjustment_table) }
		{
			if (values.empty()) {
				throw std::invalid_argument(
					"The risk factors adjustment table must not be empty.");
			}
			else if (values.rows() != 2) {
				throw std::invalid_argument(
					"The risk factors adjustment definition must contain two tables.");
			}

			if (!values.contains(core::Gender::male)) {
				throw std::invalid_argument("Missing the required adjustment table for male.");
			}
			else if (!values.contains(core::Gender::female)) {
				throw std::invalid_argument("Missing the required adjustment table for female.");
			}
		}

		FactorAdjustmentTable values{};
	};

	struct FirstMoment
	{
		bool empty() const noexcept { return count_ < 1; }
		int count() const noexcept { return count_; }
		double sum() const noexcept { return sum_; }
		double mean() const noexcept {
			if (count_ > 0) {
				return sum_ / count_;
			}

			return 0.0;
		}

		void append(double value) noexcept {
			count_++;
			sum_ += value;
		}

		void clear() noexcept {
			count_ = 0;
			sum_ = 0.0;
		}

		auto operator<=>(const FirstMoment&) const = default;

	private:
		int count_{};
		double sum_{};
	};
}
