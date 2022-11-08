#pragma once

#include <algorithm>
#include <vector>
#include <optional>

#include "column.h"
#include "column_iterator.h"

namespace hgps {
	namespace core {

		template<typename TYPE>
		class PrimitiveDataTableColumn : public DataTableColumn {
		public:
			using value_type = TYPE;
			using IteratorType = DataTableColumnIterator<PrimitiveDataTableColumn<TYPE>>;

			explicit PrimitiveDataTableColumn(std::string&& name, std::vector<TYPE>&& data)
				: DataTableColumn(), name_{ name }, data_{ data }
			{
				if (name_.length() < 2 || !std::isalpha(name_.front())) {
					throw std::invalid_argument(
						"Invalid column name: minimum length of two and start with alpha character.");
				}

				null_count_ = 0;
			}

			explicit PrimitiveDataTableColumn(std::string&& name, std::vector<TYPE>&& data, std::vector<bool>&& null_bitmap)
				: DataTableColumn(), name_{ name }, data_{ data }, null_bitmap_{ null_bitmap }
			{
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

			std::string type() const noexcept override { return typeid(TYPE).name(); }

			std::string name() const noexcept override { return name_; }

			std::size_t null_count() const noexcept override { return null_count_; }

			std::size_t length() const noexcept override { return data_.size(); }

			/// @brief Checks if value at column index is null
			/// @param index of the columns to check
			/// @return true if the value is null, otherwise false.
			bool is_null(std::size_t index) const noexcept override {
				// Bound checks hit performance!
				if (index >= length()) {
					return true;
				}

				return !null_bitmap_.empty() && !null_bitmap_[index];
			}

			/// @brief Checks if value at column index is null, does not bounds check
			/// @param index of the columns to check
			/// @return true if the value is null, otherwise false.
			bool is_valid(std::size_t index) const noexcept override {
				// Bound checks hit performance!
				if (index >= length()) {
					return false;
				}

				return null_bitmap_.empty() || null_bitmap_[index];
			}

			const std::any value(std::size_t index) const noexcept override {
				if (is_valid(index)) {
					return data_[index];
				}

				return std::any();
			}

			const std::optional<value_type> value_safe(const std::size_t index) const noexcept {
				if (is_valid(index)) {
					return data_[index];
				}

				return std::nullopt;
			}

			const value_type value_unsafe(const std::size_t index) const {
					return data_[index];
			}

			IteratorType begin() const { return IteratorType(*this); }

			IteratorType end() const { return IteratorType(*this, length()); }

		private:
			std::string name_;
			std::vector<TYPE> data_;
			std::vector<bool> null_bitmap_{};
			std::size_t null_count_ = 0;
		};

		class StringDataTableColumn : public PrimitiveDataTableColumn<std::string> {
		public:
			StringDataTableColumn(std::string&& name, std::vector<std::string>&& data)
				: PrimitiveDataTableColumn{ std::move(name), std::move(data) }
			{}

			StringDataTableColumn(std::string&& name, std::vector<std::string>&& data, std::vector<bool>&& null_bitmap)
				: PrimitiveDataTableColumn{ std::move(name), std::move(data), std::move(null_bitmap) }
			{}

			std::string type() const noexcept override { return "string"; }

			void accept(DataTableColumnVisitor& visitor) const override { visitor.visit(*this); }
		};
	}
}