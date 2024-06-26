#include "univariate_visitor.h"

namespace hgps {
core::UnivariateSummary UnivariateVisitor::get_summary() { return summary_; }

void UnivariateVisitor::visit(const core::StringDataTableColumn &column) {
    throw std::invalid_argument("Statistical summary is not available for column type string: " +
                                column.name());
}

void UnivariateVisitor::visit(const core::FloatDataTableColumn &column) {
    summary_ = core::UnivariateSummary(column.name());
    if (column.null_count() > 0) {
        for (const auto &v : column) {
            summary_.append(v);
        }

        return;
    }

    for (const auto &v : column) {
        // We know this won't be nullopt as we've checked null_count() above
        summary_.append(v.value()); // NOLINT(bugprone-unchecked-optional-access)
    }
}

void UnivariateVisitor::visit(const core::DoubleDataTableColumn &column) {
    summary_ = core::UnivariateSummary(column.name());
    if (column.null_count() > 0) {
        for (const auto &v : column) {
            summary_.append(v);
        }

        return;
    }

    for (const auto &v : column) {
        // We know this won't be nullopt as we've checked null_count() above
        summary_.append(v.value()); // NOLINT(bugprone-unchecked-optional-access)
    }
}

void UnivariateVisitor::visit(const core::IntegerDataTableColumn &column) {
    summary_ = core::UnivariateSummary(column.name());
    if (column.null_count() > 0) {
        for (const auto &v : column) {
            summary_.append(v);
        }

        return;
    }

    for (const auto &v : column) {
        // We know this won't be nullopt as we've checked null_count() above
        summary_.append(v.value()); // NOLINT(bugprone-unchecked-optional-access)
    }
}
} // namespace hgps
