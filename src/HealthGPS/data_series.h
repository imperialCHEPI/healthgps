#pragma once
#include "HealthGPS.Core/forward_type.h"

#include <map>
#include <string>
#include <vector>

namespace hgps {

/// @brief Defines the Simulation results container for time series data
///
/// @details Channels with the time series result data are stored by Gender
/// and Identifier. The collection of channels is dynamic, but all channels
/// must contain exact the same number of data points, sample size, as given
/// during initialisation.
class DataSeries {
  public:
    DataSeries() = delete;

    /// @brief Initialises a new instance of the DataSeries class.
    /// @param sample_size The channels sample size, number of data points.
    DataSeries(std::size_t sample_size);

    /// @brief Gets a reference to a channel by Gender and identifier
    /// @param gender The gender enumeration
    /// @param key The channel identifier
    /// @return The channel data
    /// @throws std::out_of_range if the container does not have a
    /// channel with the specified age and key
    std::vector<double> &operator()(core::Gender gender, const std::string &key);
    
    /// @brief Safe wrapper for vector access with bounds checking and logging
    /// @param vec The vector to access
    /// @param index The index to access
    /// @param context Context string for logging
    /// @return Reference to element at index
    /// @throws std::out_of_range if index is out of bounds
    static double &safe_at(std::vector<double> &vec, std::size_t index, const char *context);

    /// @brief Gets a reference to a channel by Gender and identifier
    /// @param gender The gender enumeration
    /// @param key The channel identifier
    /// @return The channel data
    /// @throws std::out_of_range if the container does not have a
    /// channel with the specified age and key
    std::vector<double> &at(core::Gender gender, const std::string &key);

    /// @brief Gets a read-only reference to a channel by Gender and identifier
    /// @param gender The gender enumeration
    /// @param key The channel identifier
    /// @return The channel data
    /// @throws std::out_of_range if the container does not have a
    /// channel with the specified age and key
    const std::vector<double> &at(core::Gender gender, const std::string &key) const;

    /// @brief Gets a reference to a channel by Gender, Income and identifier
    /// @param gender The gender enumeration
    /// @param income The income enumeration
    /// @param key The channel identifier
    /// @return The channel data
    /// @throws std::out_of_range if the container does not have a
    /// channel with the specified gender, income and key
    std::vector<double> &at(core::Gender gender, core::Income income, const std::string &key);

    /// @brief Gets a read-only reference to a channel by Gender, Income and identifier
    /// @param gender The gender enumeration
    /// @param income The income enumeration
    /// @param key The channel identifier
    /// @return The channel data
    /// @throws std::out_of_range if the container does not have a
    /// channel with the specified gender, income and key
    const std::vector<double> &at(core::Gender gender, core::Income income,
                                  const std::string &key) const;

    /// @brief Gets the collection of channel identifiers
    /// @return The collection of channel identifiers
    const std::vector<std::string> &channels() const noexcept;

    /// @brief Adds a new channel to the collection
    /// @param key The new channel unique identifier
    /// @throws std::logic_error for duplicated channel identifier
    void add_channel(std::string key);

    /// @brief Adds multiple channels to the collection
    /// @param keys The new channels identifier
    /// @throws std::logic_error for duplicated channel identifier
    void add_channels(const std::vector<std::string> &keys);

    /// @brief Adds income-based channels to the collection
    /// @param keys The new income-based channels identifier
    /// @throws std::logic_error for duplicated channel identifier
    void add_income_channels(const std::vector<std::string> &keys);

    /// @brief Adds income-based channels for specific income categories
    void add_income_channels_for_categories(const std::vector<std::string> &keys,
                                            const std::vector<core::Income> &income_categories);

    /// @brief Gets the size of the channels collection
    /// @return Number of channels in dataset
    std::size_t size() const noexcept;

    /// @brief Gets the channels sample size, number of points per channel.
    /// @return The channels sample size value
    std::size_t sample_size() const noexcept;

    /// @brief Gets available income categories from the data
    /// @return Vector of available income categories
    std::vector<core::Income> get_available_income_categories() const;

    /// @brief Checks if an income category exists for a given gender
    /// @param gender The gender enumeration
    /// @param income The income enumeration
    /// @return true if the income category exists, false otherwise
    bool has_income_category(core::Gender gender, core::Income income) const;

    /// @brief Converts income category enum to string representation
    /// @param income The income enumeration
    /// @return String representation of the income category
    std::string income_category_to_string(core::Income income) const;

  private:
    std::size_t sample_size_;
    std::vector<std::string> channels_;
    std::map<core::Gender, std::map<std::string, std::vector<double>>> data_;
    std::map<core::Gender, std::map<core::Income, std::map<std::string, std::vector<double>>>>
        income_data_;
};
} // namespace hgps
