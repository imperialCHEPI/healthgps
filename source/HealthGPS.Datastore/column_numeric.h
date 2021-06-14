#pragma once

#include <vector>

#include "column.h"

namespace hgps {
    namespace data {

        template<Numerical TYPE>
        class NumericDataTableColumn : public DataTableColumn {
        public:
            using value_type = TYPE;
            using IteratorType = DataTableColumnIterator<NumericDataTableColumn<TYPE>>;

            explicit NumericDataTableColumn(std::string&& name, std::vector<TYPE>&& data, std::vector<bool>&& null_bitmap)
                : DataTableColumn(), name_{ name }, data_{ data }, null_bitmap_{ null_bitmap }
            {
                // TODO: disable public construction and always use the builder
                if (name_.length() < 2 || !std::isalpha(name_.front())) {
                    throw std::invalid_argument(
                        "Invalid column name: minimum length of two and start with alpha character.");
                }

                if (data_.size() != null_bitmap_.size()) {
                    throw std::invalid_argument(
                        "Input vectors size mismatch, the data and valid vectors size must be the same.");
                }

                null_count_ = std::count(null_bitmap_.begin(), null_bitmap_.end(), false);
            }

            const std::type_info& type() const noexcept override { return typeid(TYPE); }

            const std::string name() const noexcept override { return name_; }

            const std::size_t null_count() const noexcept override { return null_count_; }

            const std::size_t length() const noexcept override { return data_.size(); }

            const bool is_null(std::size_t index) const noexcept override {
                // outside bound is null
                if (index < length()) {
                    return !null_bitmap_[index];
                }

                return true;
            }

            const std::optional<value_type> value(const std::size_t index) const {
                if (index < length() && null_bitmap_[index]) {
                    return data_[index];
                }
                
                return std::nullopt;
            }

            IteratorType begin() const { return IteratorType(*this); }

            IteratorType end() const { return IteratorType(*this, length()); }

        private:
            const std::string name_;
            std::vector<TYPE> data_;
            std::vector<bool> null_bitmap_{};
            std::size_t null_count_ = 0;
        };

        class FloatDataTableColumn : public NumericDataTableColumn<float> {
        public:
            FloatDataTableColumn(std::string&& name, std::vector<float>&& data, std::vector<bool>&& null_bitmap)
                : NumericDataTableColumn{ std::move(name), std::move(data), std::move(null_bitmap) }
            {}

            void accept(DataTableColumnVisitor& visitor) const override { visitor.visit(*this); }
        };

        class DoubleDataTableColumn : public NumericDataTableColumn<double> {
        public:

            DoubleDataTableColumn(std::string&& name, std::vector<double>&& data, std::vector<bool>&& null_bitmap)
                : NumericDataTableColumn{ std::move(name), std::move(data), std::move(null_bitmap) }
            {}

            void accept(DataTableColumnVisitor& visitor) const override { visitor.visit(*this); }
        };

        class IntegerDataTableColumn : public NumericDataTableColumn<int> {
        public:
            IntegerDataTableColumn(std::string&& name, std::vector<int>&& data, std::vector<bool>&& null_bitmap)
                : NumericDataTableColumn{ std::move(name), std::move(data), std::move(null_bitmap) }
            {}

            void accept(DataTableColumnVisitor& visitor) const override { visitor.visit(*this); }
        };
    }
}