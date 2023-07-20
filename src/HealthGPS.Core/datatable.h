#pragma once

#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <vector>

#include "column.h"

namespace hgps::core {

/// @brief Defines a Datatable for in memory data class
class DataTable {
  public:
    /// @brief DataTable columns iterator type
    using IteratorType = std::vector<std::unique_ptr<DataTableColumn>>::const_iterator;

    /// @brief Gets the number of columns
    /// @return Number of columns
    std::size_t num_columns() const noexcept;

    /// @brief Gets the number of rows
    /// @return Number of rows
    std::size_t num_rows() const noexcept;

    /// @brief Gets the collection of columns name
    /// @return Columns name collection
    std::vector<std::string> names() const;

    /// @brief Adds a new column to the table
    /// @param column The column to add
    /// @throws std::invalid_argument for duplicated column name or size mismatch.
    void add(std::unique_ptr<DataTableColumn> column);

    /// @brief Gets the column at a given index
    /// @param index Column index
    /// @return The column instance
    /// @throws std::out_of_range for column index outside the range.
    const DataTableColumn &column(std::size_t index) const;

    /// @brief Gets the column by name
    /// @param name The column name
    /// @return The column instance
    /// @throws std::out_of_range for column name not found.
    const DataTableColumn &column(const std::string &name) const;

    /// @brief Gets the column by name, also checking if the column exists
    /// @param name The column name
    /// @return A pair containing a success flag, and the column instance on success.
    const std::optional<std::reference_wrapper<const DataTableColumn>>
    column_if_exists(const std::string &name) const;

    /// @brief Gets the iterator to the first column of the table.
    /// @return An iterator to the beginning
    IteratorType cbegin() const noexcept { return columns_.cbegin(); }

    /// @brief Gets the iterator element following the last column of the table.
    /// @return An iterator to the end
    IteratorType cend() const noexcept { return columns_.cend(); }

    /// @brief Creates a string representation of the DataTable structure
    /// @return The structure string representation
    std::string to_string() const noexcept;

  private:
    std::vector<std::string> names_{};
    std::unordered_map<std::string, std::size_t> index_{};
    std::vector<std::unique_ptr<DataTableColumn>> columns_{};
    size_t rows_count_ = 0;
    std::mutex sync_mtx_{};
};
} // namespace hgps::core

/// @brief Output streams operator for DataTable type.
/// @param stream The stream to output
/// @param table The DataTable instance
/// @return The output stream
std::ostream &operator<<(std::ostream &stream, const hgps::core::DataTable &table);
