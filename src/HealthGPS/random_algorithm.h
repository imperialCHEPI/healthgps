#pragma once

#include "randombit_generator.h"
#include <functional>

namespace hgps {
/// @brief General purpose Random number generator algorithms
class Random {
  public:
    Random() = delete;
    /// @brief Initialise a new instance of the Random class
    /// @param generator Underline pseudo-random number engine instance
    Random(RandomBitGenerator &generator);

    /// @brief Generates the next random integer in the sequence
    /// @return A random integer value in range [\c min(), \c max())
    int next_int();

    /// @brief Generates the next random integer in sequence in range [min(), max_value)
    /// @param max_value The maximum value
    /// @return A random integer value in range [min(), max_value)
    int next_int(int max_value);

    /// @brief Generates the next random integer in sequence in range [min_value, max_value)
    /// @param min_value The minimum value
    /// @param max_value The maximum value
    /// @return A random integer value in range [min_value, max_value)
    int next_int(int min_value, int max_value);

    /// @brief Generates a random floating point number in range [0,1)
    /// @return A floating point value in range [0,1).
    double next_double() noexcept;

    /// @brief Generates the next random number from a standard normal distribution
    /// @return The generated floating point random number
    double next_normal();

    /// @brief Generates the next random number from a normal distribution
    /// @param mean The mean parameter
    /// @param standard_deviation The standard deviation parameter
    /// @return The generated floating point random number
    double next_normal(double mean, double standard_deviation);

    /// @brief Samples a random value from an empirical distribution
    /// @param values The empirical distribution values
    /// @param cdf The cumulative distribution values
    /// @return The random select value
    int next_empirical_discrete(const std::vector<int> &values, const std::vector<float> &cdf);

    /// @brief Samples a random value from an empirical distribution
    /// @param values The empirical distribution values
    /// @param cdf The cumulative distribution values
    /// @return The random select value
    int next_empirical_discrete(const std::vector<int> &values, const std::vector<double> &cdf);

  private:
    std::reference_wrapper<RandomBitGenerator> engine_;
    int next_int_internal(int min_value, int max_value);
    double next_uniform_internal(double min_value, double max_value);
    double next_normal_internal(double mean, double standard_deviation);
};
} // namespace hgps
