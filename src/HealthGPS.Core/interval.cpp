#include "interval.h"

namespace hgps::core {

IntegerInterval parse_integer_interval(const std::string_view &value,
                                       const std::string_view delims) {
    auto parts = split_string(value, delims);
    if (parts.size() == 2) {
        return IntegerInterval(std::stoi(parts[0].data()), std::stoi(parts[1].data()));
    }

    throw std::invalid_argument(
        fmt::format("Input value:'{}' does not have the right format: xx-xx.", value));
}

FloatInterval parse_float_interval(const std::string_view &value, const std::string_view delims) {
    auto parts = split_string(value, delims);
    if (parts.size() == 2) {
        return FloatInterval(std::stof(parts[0].data()), std::stof(parts[1].data()));
    }

    throw std::invalid_argument(
        fmt::format("Input value:'{}' does not have the right format: xx-xx.", value));
}

DoubleInterval parse_double_interval(const std::string_view &value, const std::string_view delims) {
    auto parts = split_string(value, delims);
    if (parts.size() == 2) {
        return DoubleInterval(std::stod(parts[0].data()), std::stod(parts[1].data()));
    }

    throw std::invalid_argument(
        fmt::format("Input value:'{}' does not have the right format: xx-xx.", value));
}
} // namespace hgps::core