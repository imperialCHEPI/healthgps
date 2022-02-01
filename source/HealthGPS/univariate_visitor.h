#pragma once

#include <stdexcept>

#include "HealthGPS.Core/visitor.h"
#include "HealthGPS.Core/column_numeric.h"
#include "HealthGPS.Core/univariate_summary.h"

namespace hgps {
	class UnivariateVisitor : public core::DataTableColumnVisitor {
	public:
		core::UnivariateSummary get_summary();

		void visit(const core::StringDataTableColumn& column) override;

		void visit(const core::FloatDataTableColumn& column) override;

		void visit(const core::DoubleDataTableColumn& column) override;

		void visit(const core::IntegerDataTableColumn& column) override;

	private:
		core::UnivariateSummary summary_;
	};
}