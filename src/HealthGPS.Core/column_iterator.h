#pragma once

#include <iterator>

namespace hgps::core {

	namespace detail {

		/// @brief Defines the default column value accessor type
		/// @tparam ColumnType The column type
		template <typename ColumnType>
		struct DefaultValueAccessor {
			/// @brief The column value type
			using ValueType = decltype(std::declval<ColumnType>().value_safe(0));

			/// @brief Gets a value from the target column
			/// @param column The column instance
			/// @param index The value index
			/// @return The respective column value
			ValueType operator()(const ColumnType& column, std::size_t index) {
				return column.value_safe(index);
			}
		};
	}  // namespace detail

	/// @brief DataTable column iterator data type class
	/// @tparam ColumnType Column type
	/// @tparam ValueAccessor Column value accessor type
	template <typename ColumnType,
		typename ValueAccessor = detail::DefaultValueAccessor<ColumnType>>
		class DataTableColumnIterator {
		public:
			using value_type = typename ValueAccessor::ValueType;
			using difference_type = std::size_t;
			using reference = value_type&;
			using iterator_category = std::random_access_iterator_tag;

			/// @brief Initialise a new instance of the DataTableColumnIterator{ColumnType, ValueAccessor} class.
			/// @details Some algorithms need to default-construct an iterator.
			DataTableColumnIterator() : column_(nullptr), index_(0) {}

			/// @brief Initialise a new instance of the DataTableColumnIterator{ColumnType, ValueAccessor} class.
			/// @param column The column instance to iterate
			/// @param index Current index
			explicit DataTableColumnIterator(const ColumnType& column, std::size_t index = 0)
				: column_(&column), index_(index) {}

			/// @brief Gets the current iterator index
			/// @return Current iterator index
			std::size_t index() const { return index_; }

			/// @brief Gets the current iterator value
			/// @return Iterator value access
			value_type operator*() const {
				return column_->is_null(index_) ? value_type{} : column_->value_safe(index_).value();
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

/// @brief Global namespace
namespace std {

	/// @brief DataTable column iterator traits
	/// @tparam ColumnType Column type
	template <typename ColumnType>
	struct iterator_traits<::hgps::core::DataTableColumnIterator<ColumnType>> {
		using IteratorType = ::hgps::core::DataTableColumnIterator<ColumnType>;
		using difference_type = typename IteratorType::difference_type;
		using value_type = typename IteratorType::value_type;
		using reference = typename IteratorType::reference;
		using iterator_category = typename IteratorType::iterator_category;
	};
}