#pragma once
#include <map>

namespace hgps {

/// @brief Defines the two-dimensional map (lookup table) data type
/// @tparam TROW Rows date type
/// @tparam TCOL Columns data type
/// @tparam TCell Cell value data type
template <class TROW, class TCOL, class TCell> class Map2d {
  public:
    /// @brief Map row iterator
    using IteratorType = typename std::map<TROW, std::map<TCOL, TCell>>::iterator;

    /// @brief Read-only map iterator
    using ConstIteratorType = typename std::map<TROW, std::map<TCOL, TCell>>::const_iterator;

    /// @brief Initialises a new instance of the Map2d class.
    Map2d() = default;

    /// @brief Initialises a new instance of the Map2d class.
    /// @param data The lookup table data
    Map2d(std::map<TROW, std::map<TCOL, TCell>> &&data) noexcept : table_{std::move(data)} {}

    /// @brief Determine whether the container is empty
    /// @return true, if the container is empty; otherwise, false.
    bool empty() const noexcept { return table_.empty(); }

    /// @brief Gets the total size of the container dataset
    /// @return Total size
    std::size_t size() const noexcept { return rows() * columns(); }

    /// @brief Gets the number of rows
    /// @return Number of rows
    std::size_t rows() const noexcept { return table_.size(); }

    /// @brief Gets the number of columns
    /// @return Number of columns
    std::size_t columns() const noexcept {
        if (table_.size() > 0) {

            // Assuming that inner map has same size.
            return table_.begin()->second.size();
        }

        return 0;
    }

    /// @brief Determines whether the container contains a row
    /// @param row_key The row identifier
    /// @return true, if the contains the row; otherwise, false
    bool contains(const TROW &row_key) const { return table_.contains(row_key); }

    /// @brief Determines whether the container contains a value
    /// @param row_key The row identifier
    /// @param col_key The column identifier
    /// @return true, if the contains the row; otherwise, false
    bool contains(const TROW &row_key, const TCOL &col_key) const {
        if (table_.contains(row_key)) {
            return table_.at(row_key).contains(col_key);
        }

        return false;
    }

    /// @brief Gets a row dataset
    /// @param row_key The row identifier
    /// @return Row dataset
    std::map<TCOL, TCell> &row(const TROW &row_key) { return table_.at(row_key); }

    /// @brief Gets a read-row dataset
    /// @param row_key The row identifier
    /// @return Row dataset
    const std::map<TCOL, TCell> &row(const TROW &row_key) const { return table_.at(row_key); }

    /// @brief Gets a value at a cell by row and column with bounds checking
    /// @param row_key The row identifier
    /// @param col_key The column identifier
    /// @return The cell value
    TCell &at(const TROW &row_key, const TCOL &col_key) { return table_.at(row_key).at(col_key); }

    /// @brief Gets a read-only value at a cell by row and column with bounds checking
    /// @param row_key The row identifier
    /// @param col_key The column identifier
    /// @return The read-only cell value
    const TCell &at(const TROW &row_key, const TCOL &col_key) const {
        return table_.at(row_key).at(col_key);
    }

    /// @brief Gets an iterator to the beginning of the map
    /// @return Iterator to the first element
    IteratorType begin() noexcept { return table_.begin(); }

    /// @brief Gets an iterator to the element following the element of the map.
    /// @return Iterator to the element following the last element.
    IteratorType end() noexcept { return table_.end(); }

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

  private:
    std::map<TROW, std::map<TCOL, TCell>> table_;
};
} // namespace hgps
