#pragma once
#include <string>
#include <unordered_map>

namespace hgps {

/// @brief Defines the Simulation run-time metrics container data type
///
/// @details Dynamic runtime metrics can be consistent collected throughout
/// the simulation algorithm, these metrics are included in the simulation
/// results for analysis.
class RuntimeMetric {
  public:
    /// @brief Metric iterator
    using IteratorType = std::unordered_map<std::string, double>::iterator;

    /// @brief Read-only metric iterator
    using ConstIteratorType = std::unordered_map<std::string, double>::const_iterator;

    /// @brief Gets the number of metrics in the container.
    /// @return The number of metrics.
    std::size_t size() const noexcept;

    /// @brief Gets a reference to a metric value by identifier, no bounds checking.
    /// @param metric_key Metric identifier.
    /// @return Reference to metric value.
    double &operator[](const std::string metric_key);

    /// @brief Gets a reference to a metric value by identifier with bounds checking.
    /// @param metric_key Metric identifier.
    /// @return Reference to metric value.
    /// @throws std::out_of_range if the container does not have a metric with the specified
    /// identifier.
    double &at(const std::string metric_key);

    /// @brief Gets a read-only reference to a metric value by identifier with bounds checking.
    /// @param metric_key Metric identifier.
    /// @return Reference to metric value.
    /// @throws std::out_of_range if the container does not have a metric with the specified
    /// identifier.
    const double &at(const std::string metric_key) const;

    /// @brief Checks if the container contains a metric with specific identifier
    /// @param metric_key Metric identifier.
    /// @return true if there is such a metric; otherwise, false.
    bool contains(const std::string metric_key) const noexcept;

    /// @brief Inserts a new metric into the container constructed in-place with arguments.
    /// @param metric_key Metric identifier.
    /// @param value Metric value.
    /// @return true if insertion happened; otherwise, false.
    bool emplace(const std::string metric_key, const double value);

    /// @brief Removes a specified metric from the container.
    /// @param metric_key Metric identifier.
    /// @return true if deletion happened; otherwise, false.
    bool erase(const std::string metric_key);

    /// @brief Erases all metrics from the container.
    void clear() noexcept;

    /// @brief Resets the values of all metrics in the container to zero.
    void reset() noexcept;

    /// @brief Checks if the container has no metrics.
    /// @return true if the container is empty; otherwise, false.
    bool empty() const noexcept;

    /// @brief Gets an iterator to the beginning of the metrics
    /// @return Iterator to the first metric
    IteratorType begin() noexcept { return metrics_.begin(); }

    /// @brief Gets an iterator to the element following the last metric
    /// @return Iterator to the element following the last metric.
    IteratorType end() noexcept { return metrics_.end(); }

    /// @brief Gets a read-only iterator to the beginning of the metrics
    /// @return Iterator to the first metric
    ConstIteratorType begin() const noexcept { return metrics_.cbegin(); }

    /// @brief Gets a read-only iterator to the element following the last metric
    /// @return Iterator to the element following the last metric.
    ConstIteratorType end() const noexcept { return metrics_.cend(); }

    /// @brief Gets a read-only iterator to the beginning of the metrics
    /// @return Iterator to the first metric
    ConstIteratorType cbegin() const noexcept { return metrics_.cbegin(); }

    /// @brief Gets a read-only iterator to the element following the last metric
    /// @return Iterator to the element following the last metric.
    ConstIteratorType cend() const noexcept { return metrics_.cend(); }

  private:
    std::unordered_map<std::string, double> metrics_;
};
} // namespace hgps