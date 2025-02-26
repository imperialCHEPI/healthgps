#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <vector>

#include "column.h"
#include "forward_type.h"

namespace hgps::core {

class DataTable {
  public:
    // Define the struct inside the class
    struct DemographicCoefficients {
        double age_coefficient{0.0};
        std::unordered_map<Gender, double> gender_coefficients;
        std::unordered_map<Region, double> region_coefficients;
    };

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
    std::optional<std::reference_wrapper<const DataTableColumn>>
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

    DemographicCoefficients get_demographic_coefficients(const std::string &model_type) const;
    double calculate_probability(const DemographicCoefficients &coeffs, int age, Gender gender,
                                 Region region,
                                 std::optional<Ethnicity> ethnicity = std::nullopt) const;
    void load_demographic_coefficients(const nlohmann::json &config);
    std::unordered_map<Region, double> get_region_distribution(int age, Gender gender) const;
    std::unordered_map<Ethnicity, double> get_ethnicity_distribution(int age, Gender gender,
                                                                     Region region) const;

  private:
    // Add member variable to store the demographic coefficients
    std::unordered_map<std::string, DemographicCoefficients> demographic_coefficients_;

    std::unique_ptr<std::mutex> sync_mtx_{std::make_unique<std::mutex>()};
    std::vector<std::string> names_{};
    std::unordered_map<std::string, std::size_t> index_{};
    std::vector<std::unique_ptr<DataTableColumn>> columns_{};
    size_t rows_count_ = 0;
};

} // namespace hgps::core

/// @brief Output streams operator for DataTable type.
/// @param stream The stream to output
/// @param table The DataTable instance
/// @return The output stream
std::ostream &operator<<(std::ostream &stream, const hgps::core::DataTable &table);
