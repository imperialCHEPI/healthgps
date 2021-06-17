#pragma once

#include <iterator>

namespace hgps {
	namespace core {

		namespace detail {

			template <typename ColumnType>
			struct DefaultValueAccessor {
				using ValueType = decltype(std::declval<ColumnType>().value(0));

				ValueType operator()(const ColumnType& column, std::size_t index) {
					return column.value(index);
				}
			};
		}  // namespace detail

		template <typename ColumnType, 
			      typename ValueAccessor = detail::DefaultValueAccessor<ColumnType>>
		class DataTableColumnIterator {
		public:
			using value_type = ValueAccessor::ValueType;
			using difference_type = std::size_t;
			using pointer = value_type*;
			using reference = value_type&;
			using iterator_category = std::random_access_iterator_tag;

			// Some algorithms need to default-construct an iterator
			DataTableColumnIterator() : column_(nullptr), index_(0) {}

			explicit DataTableColumnIterator(const ColumnType& column, std::size_t index = 0)
				: column_(&column), index_(index) {}

			std::size_t index() const { return index_; }

			// Value access
			value_type operator*() const {
				return column_->is_null(index_) ? value_type{} : column_->value(index_).value();
			}

			// Forward / backward
			DataTableColumnIterator& operator--() { --index_; return *this; }
			DataTableColumnIterator& operator++() { ++index_; return *this; }
			DataTableColumnIterator operator++(int) { DataTableColumnIterator tmp(*this); ++index_; return tmp; }
			DataTableColumnIterator operator--(int) { DataTableColumnIterator tmp(*this); --index_; return tmp; }

			// Arithmetic
			difference_type operator-(const DataTableColumnIterator& other) const { return index_ - other.index_; }

			DataTableColumnIterator operator+(difference_type n) const { return DataTableColumnIterator(*column_, index_ + n); }
			DataTableColumnIterator operator-(difference_type n) const { return DataTableColumnIterator(*column_, index_ - n); }
			DataTableColumnIterator& operator+=(difference_type n) { index_ += n; return *this; }
			DataTableColumnIterator& operator-=(difference_type n) { index_ -= n; return *this; }

			friend inline DataTableColumnIterator operator+(difference_type diff,
				const DataTableColumnIterator& other) {
				return DataTableColumnIterator(*other.column_, diff + other.index_);
			}

			friend inline DataTableColumnIterator operator-(difference_type diff,
				const DataTableColumnIterator& other) {
				return DataTableColumnIterator(*other.array_, diff - other.index_);
			}

			// Comparisons
			bool operator==(const DataTableColumnIterator& other) const { return index_ == other.index_; }
			bool operator!=(const DataTableColumnIterator& other) const { return index_ != other.index_; }
			bool operator<(const DataTableColumnIterator& other) const { return index_ < other.index_; }
			bool operator>(const DataTableColumnIterator& other) const { return index_ > other.index_; }
			bool operator<=(const DataTableColumnIterator& other) const { return index_ <= other.index_; }
			bool operator>=(const DataTableColumnIterator& other) const { return index_ >= other.index_; }

		private:
			const ColumnType* column_;
			std::size_t index_;
		};
	}
}

namespace std {

	template <typename ColumnType>
	struct iterator_traits<::hgps::core::DataTableColumnIterator<ColumnType>> {
		using IteratorType = ::hgps::core::DataTableColumnIterator<ColumnType>;
		using difference_type = typename IteratorType::difference_type;
		using value_type = typename IteratorType::value_type;
		using pointer = typename IteratorType::pointer;
		using reference = typename IteratorType::reference;
		using iterator_category = typename IteratorType::iterator_category;
	};
}