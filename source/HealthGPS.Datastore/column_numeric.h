#pragma once

#include "column_primitive.h"

namespace hgps {
    namespace data {

        class FloatDataTableColumn : public PrimitiveDataTableColumn<float> {
        public:
            FloatDataTableColumn(std::string&& name, std::vector<float>&& data)
                : PrimitiveDataTableColumn{ std::move(name), std::move(data) }
            {}

            FloatDataTableColumn(std::string&& name, std::vector<float>&& data, std::vector<bool>&& null_bitmap)
                : PrimitiveDataTableColumn{ std::move(name), std::move(data), std::move(null_bitmap) }
            {}

            void accept(DataTableColumnVisitor& visitor) const override { visitor.visit(*this); }
        };

        class DoubleDataTableColumn : public PrimitiveDataTableColumn<double> {
        public:

            DoubleDataTableColumn(std::string&& name, std::vector<double>&& data)
                : PrimitiveDataTableColumn{ std::move(name), std::move(data) }
            {}

            DoubleDataTableColumn(std::string&& name, std::vector<double>&& data, std::vector<bool>&& null_bitmap)
                : PrimitiveDataTableColumn{ std::move(name), std::move(data), std::move(null_bitmap) }
            {}

            void accept(DataTableColumnVisitor& visitor) const override { visitor.visit(*this); }
        };

        class IntegerDataTableColumn : public PrimitiveDataTableColumn<int> {
        public:
            IntegerDataTableColumn(std::string&& name, std::vector<int>&& data)
                : PrimitiveDataTableColumn{ std::move(name), std::move(data) }
            {}

            IntegerDataTableColumn(std::string&& name, std::vector<int>&& data, std::vector<bool>&& null_bitmap)
                : PrimitiveDataTableColumn{ std::move(name), std::move(data), std::move(null_bitmap) }
            {}

            void accept(DataTableColumnVisitor& visitor) const override { visitor.visit(*this); }
        };
    }
}