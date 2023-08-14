#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#if defined(__clang__) && __clang_major__ <= 14
#include <experimental/source_location>
using std::experimental::source_location;
#else
#include <source_location>
using std::source_location;
#endif // defined(__clang__) && __clang_major__ <= 14

namespace hgps::core {

class HgpsException : public std::runtime_error {
  public:
    HgpsException(const std::string &what_arg,
                  const source_location location = source_location::current());

    HgpsException(const char *what_arg,
                  const source_location location = source_location::current());

    const char *what() const noexcept override;

    constexpr std::uint_least32_t line() const noexcept;

    constexpr std::uint_least32_t column() const noexcept;

    constexpr const char *file_name() const noexcept;

    constexpr const char *function_name() const noexcept;

  private:
    std::string what_arg_;
    source_location location_;
};

} // namespace hgps::core
