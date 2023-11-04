#pragma once

#include <map>
#include <utility>

namespace hgps {

/// @brief Defines the two-dimensional map (lookup table) data type
/// @tparam TROW Rows date type
/// @tparam TCOL Columns data type
/// @tparam TCell Cell value data type
template <class TROW, class TCOL, class TCell> class Map2D {
  public:
    /// @brief Initialises a new instance of the Map2D class.
    Map2D() = default;

    /// @brief Initialises a new instance of the Map2D class from a standard C++ 2D mapping.
    /// @param data The lookup table data
    Map2D(std::map<TROW, std::map<TCOL, TCell>> &&data) noexcept : table_{std::move(data)} {}

    /// @brief Map row iterator
    using IteratorType = typename std::map<TROW, std::map<TCOL, TCell>>::iterator;

    /// @brief Gets an iterator to the beginning of the map
    /// @return Iterator to the first element
    IteratorType begin() noexcept { return table_.begin(); }

    /// @brief Gets an iterator to the element following the element of the map.
    /// @return Iterator to the element following the last element.
    IteratorType end() noexcept { return table_.end(); }

    /// @brief Read-only map row iterator
    using ConstIteratorType = typename std::map<TROW, std::map<TCOL, TCell>>::const_iterator;

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
    bool empty(const TROW &row_key) const { return table_.at(row_key).empty(); }

    /// @brief Gets the number of rows
    /// @return Number of rows
    std::size_t rows() const noexcept { return table_.size(); }

    /// @brief Gets the number of columns at a given row
    /// @param row_key The row identifier
    /// @return Number of columns at row
    /// @throws std::out_of_range for invalid row identifier
    std::size_t columns(const TROW &row_key) const { return table_.at(row_key).size(); }

    /// @brief Determines whether the container contains a row
    /// @param row_key The row identifier
    /// @return true, if the contains the row; otherwise, false
    /// @throws std::out_of_range for invalid row identifier
    bool contains(const TROW &row_key) const noexcept { return table_.contains(row_key); }

    /// @brief Determines whether the container contains a value
    /// @param row_key The row identifier
    /// @param col_key The column identifier
    /// @return true, if the contains the row; otherwise, false
    bool contains(const TROW &row_key, const TCOL &col_key) const {
        return table_.at(row_key).contains(col_key);
    }

    /// @brief Gets a row dataset with bounds checking
    /// @param row_key The row identifier
    /// @return Row dataset
    std::map<TCOL, TCell> &at(const TROW &row_key) { return table_.at(row_key); }

    /// @brief Gets a read-only row dataset with bounds checking
    /// @param row_key The row identifier
    /// @return Row dataset
    const std::map<TCOL, TCell> &at(const TROW &row_key) const { return table_.at(row_key); }

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
    template <class... Args> auto insert(const TROW &row_key, Args &&...args) {
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
    template <class... Args> auto emplace(const TROW &row_key, Args &&...args) {
        return table_.at(row_key).emplace(std::forward<Args>(args)...);
    }

  private:
    std::map<TROW, std::map<TCOL, TCell>> table_;
};

} // namespace hgps
