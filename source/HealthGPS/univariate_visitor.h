#pragma once

#include <stdexcept>

#include "HealthGPS.Core/visitor.h"
#include "HealthGPS.Core/column_numeric.h"
#include "HealthGPS.Core/univariate_summary.h"

namespace hgps {
	class UnivariateVisitor : public core::DataTableColumnVisitor {
	public:
		core::UnivariateSummary get_summary() {
			return summary_;
		}

		void visit(const core::StringDataTableColumn& column) override {
			throw std::invalid_argument(
				"Attempting to calculate statistical summary of strings.");
		}

		void visit(const core::FloatDataTableColumn& column) override {
			summary_ = core::UnivariateSummary(column.name());
			if (column.null_count() > 0) {
				for (auto& v : column) {
					summary_.append(v);
				}

				return;
			}

			for (auto& v : column) {
				summary_.append(v.value());
			}
		}

		void visit(const core::DoubleDataTableColumn& column) override {
			summary_ = core::UnivariateSummary(column.name());
			if (column.null_count() > 0) {
				for (auto& v : column) {
					summary_.append(v);
				}

				return;
			}

			for (auto& v : column) {
				summary_.append(v.value());
			}
		}

		void visit(const core::IntegerDataTableColumn& column) override {
			summary_ = core::UnivariateSummary(column.name());
			if (column.null_count() > 0) {
				for (auto& v : column) {
					summary_.append(v);
				}

				return;
			}

			for (auto& v : column) {
				summary_.append(v.value());
			}
		}

	private:
		core::UnivariateSummary summary_;
	};
}