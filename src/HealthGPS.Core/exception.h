#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

// HACK: Clang 14 does not support std::source_location.
#if defined(__clang__) && __clang_major__ <= 14
#include <experimental/source_location>
using std::experimental::source_location;
#else
#include <source_location>
using std::source_location;
#endif // defined(__clang__) && __clang_major__ <= 14

namespace hgps::core {

/// @brief HealthGPS base exception class, with source location information
class HgpsException : public std::runtime_error {
  public:
    /// @brief Construct a new HgpsException object
    /// @param what_arg Debug messafe for the exception
    /// @param location Source location (defaults to current location)
    HgpsException(const std::string &what_arg,
                  const source_location location = source_location::current());

    /// @brief Construct a new HgpsException object
    /// @param what_arg Debug message for the exception
    /// @param location Source location (defaults to current location)
    HgpsException(const char *what_arg,
                  const source_location location = source_location::current());

    /// @brief Gets the exception debug message
    /// @return The debug message
    const char *what() const noexcept override;

    /// @brief Gets the exception source location line
    /// @return The location line
    constexpr std::uint_least32_t line() const noexcept;

    /// @brief Gets the exception source location column
    /// @return The location column
    constexpr std::uint_least32_t column() const noexcept;

    /// @brief Gets the exception source location file name
    /// @return The location file name
    constexpr const char *file_name() const noexcept;

    /// @brief Gets the exception source location function name
    /// @return The location function name
    constexpr const char *function_name() const noexcept;

  private:
    std::string what_arg_;
    source_location location_;
};

} // namespace hgps::core
