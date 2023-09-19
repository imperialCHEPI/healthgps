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

    /// @brief Gets the size of the channels collection
    /// @return Number of channels in dataset
    std::size_t size() const noexcept;

    /// @brief Gets the channels sample size, number of points per channel.
    /// @return The channels sample size value
    std::size_t sample_size() const noexcept;

  private:
    std::size_t sample_size_;
    std::vector<std::string> channels_;
    std::map<core::Gender, std::map<std::string, std::vector<double>>> data_;
};
} // namespace hgps
