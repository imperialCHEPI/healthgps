#pragma once

#include "forward_type.h"

namespace hgps {
    namespace core {

        class DataTableColumnVisitor {
        public:
            DataTableColumnVisitor() = default;

            DataTableColumnVisitor(const DataTableColumnVisitor&) = delete;
            DataTableColumnVisitor& operator=(const DataTableColumnVisitor&) = delete;

            DataTableColumnVisitor(DataTableColumnVisitor&&) = delete;
            DataTableColumnVisitor& operator=(DataTableColumnVisitor&&) = delete;

            virtual ~DataTableColumnVisitor() {};

            virtual void visit(const StringDataTableColumn& column) = 0;

            virtual void visit(const FloatDataTableColumn& column) = 0;

            virtual void visit(const DoubleDataTableColumn& column) = 0;

            virtual void visit(const IntegerDataTableColumn& column) = 0;
        };
    }
}