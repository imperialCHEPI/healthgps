#include "exception.h"

#include <fmt/format.h>
#include <iostream>

namespace hgps::core {

HgpsException::HgpsException(const std::string &what_arg, const std::source_location location)
    : std::runtime_error{what_arg}, location_{location} {
    what_arg_ = fmt::format("[{}] Error in {}, line {}: {}", file_name(), function_name(), line(),
                            std::runtime_error::what());
}

HgpsException::HgpsException(const char *what_arg, const std::source_location location)
    : HgpsException{std::string{what_arg}, location} {}

const char *HgpsException::what() const noexcept { return what_arg_.c_str(); }

constexpr std::uint_least32_t HgpsException::line() const noexcept { return location_.line(); }

constexpr std::uint_least32_t HgpsException::column() const noexcept { return location_.column(); }

constexpr const char *HgpsException::file_name() const noexcept { return location_.file_name(); }

constexpr const char *HgpsException::function_name() const noexcept {
    return location_.function_name();
}

} // namespace hgps::core
