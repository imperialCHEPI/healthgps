#include "exception.h"

#include <fmt/format.h>

namespace hgps::core {

HgpsException::HgpsException(const std::string &what_arg, const source_location location)
    : std::runtime_error{fmt::format("{}:{}: {}", location.file_name(), location.line(), what_arg)},
      location_{location} {}

constexpr std::uint_least32_t HgpsException::line() const noexcept { return location_.line(); }

constexpr std::uint_least32_t HgpsException::column() const noexcept { return location_.column(); }

constexpr const char *HgpsException::file_name() const noexcept { return location_.file_name(); }

constexpr const char *HgpsException::function_name() const noexcept {
    return location_.function_name();
}

} // namespace hgps::core
