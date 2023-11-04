#pragma once

#include <map>
#include <unordered_map>
#include <utility>

namespace hgps {

/// @brief Defines the two-dimensional map (lookup table) data type
/// @tparam TMap underlying standard map type
/// @tparam TRow Rows date type
/// @tparam TCol Columns data type
/// @tparam TCell Cell value data type
template <template <class, class> class TMap, class TRow, class TCol, class TCell> class Map2d {
  public:
    /// @brief Initialises a new instance of the Map2d class.
    Map2d() = default;

    /// @brief Initialises a new instance of the Map2d class from a standard C++ 2D mapping.
    /// @param data The lookup table data
    Map2d(TMap<TRow, TMap<TCol, TCell>> &&data) noexcept : table_{std::move(data)} {}

    /// @brief Map row iterator
    using IteratorType = typename TMap<TRow, TMap<TCol, TCell>>::iterator;

    /// @brief Gets an iterator to the beginning of the map
    /// @return Iterator to the first element
    IteratorType begin() noexcept { return table_.begin(); }

    /// @brief Gets an iterator to the element following the element of the map.
    /// @return Iterator to the element following the last element.
    IteratorType end() noexcept { return table_.end(); }

    /// @brief Read-only map row iterator
    using ConstIteratorType = typename TMap<TRow, TMap<TCol, TCell>>::const_iterator;

    /// @brief Gets an read-only iterator to the beginning of the map
    /// @return Iterator to the first element
    ConstIteratorType begin() const noexcept { return table_.cbegin(); }

    /// @brief Gets an read-only iterator to the element following the element of the map.
    /// @return Iterator to the element following the last element.
    ConstIteratorType end() const noexcept { return table_.cend(); }

    /// @brief Gets an read-only iterator to the beginning of the map
    /// @return Iterator to the first element
    ConstIteratorType cbegin() const noexcept { return table_.cbegin(); }

    /// @brief Gets an read-only iterator to the element following the element of the map.
    /// @return Iterator to the element following the last element.
    ConstIteratorType cend() const noexcept { return table_.cend(); }

    /// @brief Determine whether the container is empty
    /// @return true, if the container is empty; otherwise, false.
    bool empty() const noexcept { return table_.empty(); }

    /// @brief Determine whether a given row container is empty
    /// @param row_key The row identifier
    /// @return true, if the row container is empty; otherwise, false.
    /// @throws std::out_of_range for invalid row identifier
    bool empty(const TRow &row_key) const { return table_.at(row_key).empty(); }

    /// @brief Gets the number of rows
    /// @return Number of rows
    std::size_t rows() const noexcept { return table_.size(); }

    /// @brief Gets the number of columns at a given row
    /// @param row_key The row identifier
    /// @return Number of columns at row
    /// @throws std::out_of_range for invalid row identifier
    std::size_t columns(const TRow &row_key) const { return table_.at(row_key).size(); }

    /// @brief Determines whether the container contains a row
    /// @param row_key The row identifier
    /// @return true, if the contains the row; otherwise, false
    /// @throws std::out_of_range for invalid row identifier
    bool contains(const TRow &row_key) const noexcept { return table_.contains(row_key); }

    /// @brief Determines whether the container contains a value
    /// @param row_key The row identifier
    /// @param col_key The column identifier
    /// @return true, if the contains the row; otherwise, false
    bool contains(const TRow &row_key, const TCol &col_key) const {
        return table_.at(row_key).contains(col_key);
    }

    /// @brief Gets a row dataset with bounds checking
    /// @param row_key The row identifier
    /// @return Row dataset
    TMap<TCol, TCell> &at(const TRow &row_key) { return table_.at(row_key); }

    /// @brief Gets a read-only row dataset with bounds checking
    /// @param row_key The row identifier
    /// @return Row dataset
    const TMap<TCol, TCell> &at(const TRow &row_key) const { return table_.at(row_key); }

    /// @brief Gets a value at a cell by row and column with bounds checking
    /// @param row_key The row identifier
    /// @param col_key The column identifier
    /// @return The cell value
    TCell &at(const TRow &row_key, const TCol &col_key) { return table_.at(row_key).at(col_key); }

    /// @brief Gets a read-only value at a cell by row and column with bounds checking
    /// @param row_key The row identifier
    /// @param col_key The column identifier
    /// @return The read-only cell value
    const TCell &at(const TRow &row_key, const TCol &col_key) const {
        return table_.at(row_key).at(col_key);
    }

    /// @brief Insert a row into the table
    /// @param ...args The arguments to forward to the row insert method
    /// @return Return type for the underlying map's insert method
    template <class... Args> auto insert(Args &&...args) noexcept {
        return table_.insert(std::forward<Args>(args)...);
    }

    /// @brief Insert a column into a given row of the table
    /// @param row_key The row identifier
    /// @param ...args The arguments to forward to the column insert method
    /// @return Return type for the underlying map's insert method
    /// @throws std::out_of_range for invalid row identifier
    template <class... Args> auto insert(const TRow &row_key, Args &&...args) {
        return table_.at(row_key).insert(std::forward<Args>(args)...);
    }

    /// @brief Emplace a row into the table
    /// @param ...args The arguments to forward to the row emplace method
    /// @return Return type for the underlying map's emplace method
    template <class... Args> auto emplace(Args &&...args) noexcept {
        return table_.emplace(std::forward<Args>(args)...);
    }

    /// @brief Emplace a column into a given row of the table
    /// @param row_key The row identifier
    /// @param ...args The arguments to forward to the column emplace method
    /// @return Return type for the underlying map's emplace method
    /// @throws std::out_of_range for invalid row identifier
    template <class... Args> auto emplace(const TRow &row_key, Args &&...args) {
        return table_.at(row_key).emplace(std::forward<Args>(args)...);
    }

  private:
    TMap<TRow, TMap<TCol, TCell>> table_;
};

/// @brief Defines the two-dimensional unordered map data type
template <class TRow, class TCol, class TCell>
using UnorderedMap2d = Map2d<std::unordered_map, TRow, TCol, TCell>;

/// @brief Defines the two-dimensional ordered map data type
template <class TRow, class TCol, class TCell>
using OrderedMap2d = Map2d<std::map, TRow, TCol, TCell>;

} // namespace hgps
