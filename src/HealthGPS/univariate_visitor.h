#pragma once

#include <stdexcept>

#include "HealthGPS.Core/visitor.h"
#include "HealthGPS.Core/column_numeric.h"
#include "HealthGPS.Core/univariate_summary.h"

namespace hgps {

	/// @brief Implements an core::UnivariateSummary visitor for core::DataTable columns
	///
	/// @details Creates univariate statistical summaries for core::DataTable
	/// columns of different data types.
	class UnivariateVisitor : public core::DataTableColumnVisitor {
	public:
		/// @brief Gets the resulting core::UnivariateSummary instance
		/// @return The univariate summary for visited column
		core::UnivariateSummary get_summary();

		/// @copydoc core::DataTableColumnVisitor::visit
		/// @throws std::invalid_argument for unsupported summary for column type string.
		void visit(const core::StringDataTableColumn& column) override;

		void visit(const core::FloatDataTableColumn& column) override;

		void visit(const core::DoubleDataTableColumn& column) override;

		void visit(const core::IntegerDataTableColumn& column) override;

	private:
		core::UnivariateSummary summary_;
	};
}