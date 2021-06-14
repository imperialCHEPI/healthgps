#pragma once

#include <typeinfo>
#include <string>

#include "visitor.h"

namespace hgps {
    namespace data {

        class DataTableColumn {
        public:
            DataTableColumn() = default;

            DataTableColumn(const DataTableColumn&) = delete;
            DataTableColumn& operator=(const DataTableColumn&) = delete;

            DataTableColumn(DataTableColumn&&) = delete;
            DataTableColumn& operator=(DataTableColumn&&) = delete;

            virtual ~DataTableColumn() {};

            virtual const std::type_info& type() const noexcept = 0;

            virtual const std::string name() const noexcept = 0;

            virtual const std::size_t null_count() const noexcept = 0;

            virtual const std::size_t length() const noexcept = 0;

            virtual const bool is_null(std::size_t index) const noexcept = 0;

            virtual void accept(DataTableColumnVisitor& visitor) const = 0;
        };
    }
}
