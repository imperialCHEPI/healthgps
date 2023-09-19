#pragma once
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace hgps::core {

/// @brief Defines an univariate statistical summary data type
///
/// @details Calculates running summaries using statistical moments
/// for constant storage size without storing the data points. Null
/// values can be counted but are not included in any calculations.
class UnivariateSummary {
  public:
    /// @brief Initialises a new instance of the UnivariateSummary class.
    UnivariateSummary();

    /// @brief Initialises a new instance of the UnivariateSummary class.
    /// @param name The factor or variable name
    UnivariateSummary(std::string name);

    /// @brief Initialises a new instance of the UnivariateSummary class.
    /// @param values The values to summary
    UnivariateSummary(const std::vector<double> &values);

    /// @brief Initialises a new instance of the UnivariateSummary class.
    /// @param name The factor or variable name
    /// @param values The values to summary
    UnivariateSummary(std::string name, const std::vector<double> &values);

    /// @brief Gets the factor or variable name
    /// @return The name identification
    std::string name() const noexcept;

    /// @brief Determine whether the summary is empty
    /// @return true if the summary has is empty; otherwise, false.
    bool is_empty() const noexcept;

    /// @brief Gets the number of valid data points included in the summary
    /// @return Number of valid data points
    unsigned int count_valid() const noexcept;

    /// @brief Gets the number of null data points, not included in the summary
    /// @return Number of valid data points
    unsigned int count_null() const noexcept;

    /// @brief Gets the total number of data points
    /// @return Total number of data points
    unsigned int count_total() const noexcept;

    /// @brief Gets the minimum
    /// @return Minimum value
    double min() const noexcept;

    /// @brief Gets the maximum
    /// @return Maximum value
    double max() const noexcept;

    /// @brief Gets the summary data range
    /// @return Range value
    double range() const noexcept;

    /// @brief Gets the sum value
    /// @return Sum value
    double sum() const noexcept;

    /// @brief Gets the average or mean
    /// @return Average value
    double average() const noexcept;

    /// @brief Gets the sample variance
    /// @return Variance value
    double variance() const noexcept;

    /// @brief Gets the sample standard deviation
    /// @return Standard deviation value
    double std_deviation() const noexcept;

    /// @brief Gets the standard error
    /// @return Standard error value
    double std_error() const noexcept;

    /// @brief Gets the kurtosis
    /// @return Kurtosis value
    double kurtosis() const noexcept;

    /// @brief Gets the skewness
    /// @return Skewness value
    double skewness() const noexcept;

    /// @brief Clears current summary (empty)
    void clear() noexcept;

    /// @brief Append a new data point to the summary
    /// @param value The value to add
    void append(double value) noexcept;

    /// @brief Append a new data point or null value to the summary
    /// @param option The optional value to add
    void append(const std::optional<double> &option) noexcept;

    /// @brief Append multiple data points to the summary
    /// @param values The values to add
    void append(const std::vector<double> &values) noexcept;

    /// @brief Append a single null value to the summary
    void append_null() noexcept;

    /// @brief Append multiple null values to the summary
    /// @param count The number of null values to add
    void append_null(unsigned int count) noexcept;

    /// @brief Convert this instance to a string representation
    /// @return The equivalent string representation
    std::string to_string() const noexcept;

    /// @brief Output streams operator for UnivariateSummary type.
    /// @param stream The stream to output
    /// @param summary The UnivariateSummary instance
    /// @return The output stream
    friend std::ostream &operator<<(std::ostream &stream, const UnivariateSummary &summary);

  private:
    std::string name_;
    unsigned int null_count_{};
    double min_{};
    double max_{};
    double moments_[5]{};
};
} // namespace hgps::core
