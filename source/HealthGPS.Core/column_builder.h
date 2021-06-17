#pragma once
#include <memory>
#include <limits>
#include <vector>
#include <exception>

#include "column.h"

namespace hgps {
	namespace core {

		template<typename ColumnType>
		class PrimitiveDataTableColumnBuilder {

		public:
			using value_type = ColumnType::value_type;

			PrimitiveDataTableColumnBuilder() = delete;

			explicit PrimitiveDataTableColumnBuilder(std::string name) : name_{ name }
			{
				if (name_.length() < 2 || !std::isalpha(name_.front())) {
					throw std::invalid_argument(
						"Invalid column name: minimum length of two and start with alpha character.");
				}
			}

			std::size_t length() const { return data_.size(); }

			std::size_t null_count() const { return null_count_; }

			std::size_t capacity() const { return data_.capacity(); }

			void reserve(std::size_t capacity) { return data_.reserve(capacity); }

			void append_null() { append_null_internal(1); }

			void append_null(const std::size_t length) { append_null_internal(length); }

			void append(const value_type value) { append_internal(value); }

			value_type value(std::size_t index) const { return data_[index]; }

			value_type operator[](std::size_t index) const { return value(index); }

			value_type& operator[](std::size_t index) { return data_[index]; }

			void reset() {
				data_.clear();
				null_bitmap_.clear();
				null_count_ = 0;
			}

			std::unique_ptr<ColumnType> build() {
				data_.shrink_to_fit();
				null_bitmap_.shrink_to_fit();

				if (null_count_ > 0) {

					// Contains null data, use null bitmap
					return std::make_unique<ColumnType>(
						std::move(name_),
						std::move(data_),
						std::move(null_bitmap_));
				}

				// Full vector, no need for null bitmap
				return std::make_unique<ColumnType>(std::move(name_), std::move(data_));
			}

		private:
			std::string name_;
			std::vector<value_type> data_{};
			std::vector<bool> null_bitmap_{};
			std::size_t null_count_ = 0;

			void append_null_internal(const std::size_t length) {
				null_count_ += length;
				data_.insert(data_.end(), length, value_type{});
				null_bitmap_.insert(null_bitmap_.end(), length, false);
			}

			void append_internal(const value_type value) {
				data_.push_back(value);
				null_bitmap_.push_back(true);
			}
		};

		// Builders
		using StringDataTableColumnBuilder = PrimitiveDataTableColumnBuilder<StringDataTableColumn>;
		using FloatDataTableColumnBuilder = PrimitiveDataTableColumnBuilder<FloatDataTableColumn>;
		using DoubleDataTableColumnBuilder = PrimitiveDataTableColumnBuilder<DoubleDataTableColumn>;
		using IntegerDataTableColumnBuilder = PrimitiveDataTableColumnBuilder<IntegerDataTableColumn>;
	}
}
