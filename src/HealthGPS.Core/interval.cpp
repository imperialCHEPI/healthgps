#include "interval.h"
#include <iostream>

namespace hgps::core {

IntegerInterval parse_integer_interval(const std::string_view &value,
                                       const std::string_view delims) {
    auto parts = split_string(value, delims);
    if (parts.size() == 2) {
        try {
            int start = std::stoi(parts[0].data());
            int end = std::stoi(parts[1].data());
            return IntegerInterval(start, end);
        } catch (const std::exception &e) {
            std::cout << "\n[DEBUG] ===== CRASH DETECTED IN INTERVAL PARSING =====" << std::endl;
            std::cout << "[DEBUG] Function: parse_integer_interval" << std::endl;
            std::cout << "[DEBUG] Input value: '" << value << "'" << std::endl;
            std::cout << "[DEBUG] Delimiters: '" << delims << "'" << std::endl;
            std::cout << "[DEBUG] Split parts: [" << parts.size() << "]" << std::endl;
            std::cout << "[DEBUG] Part 0: '" << parts[0] << "'" << std::endl;
            std::cout << "[DEBUG] Part 1: '" << parts[1] << "'" << std::endl;
            std::cout << "[DEBUG] Error: " << e.what() << std::endl;
            std::cout << "[DEBUG] Exception type: " << typeid(e).name() << std::endl;
            std::cout << "[DEBUG] ===== END CRASH CONTEXT =====" << std::endl;

            std::string detailed_error = "Failed to parse integer interval from value: '" +
                                         std::string(value) +
                                         "'. Cannot convert parts to integer. Error: " + e.what();
            throw std::runtime_error(detailed_error);
        }
    }

    throw std::invalid_argument(
        fmt::format("Input value:'{}' does not have the right format: xx-xx.", value));
}

FloatInterval parse_float_interval(const std::string_view &value, const std::string_view delims) {
    auto parts = split_string(value, delims);
    if (parts.size() == 2) {
        try {
            float start = std::stof(parts[0].data());
            float end = std::stof(parts[1].data());
            return FloatInterval(start, end);
        } catch (const std::exception &e) {
            std::cout << "\n[DEBUG] ===== CRASH DETECTED IN INTERVAL PARSING =====" << std::endl;
            std::cout << "[DEBUG] Function: parse_float_interval" << std::endl;
            std::cout << "[DEBUG] Input value: '" << value << "'" << std::endl;
            std::cout << "[DEBUG] Delimiters: '" << delims << "'" << std::endl;
            std::cout << "[DEBUG] Split parts: [" << parts.size() << "]" << std::endl;
            std::cout << "[DEBUG] Part 0: '" << parts[0] << "'" << std::endl;
            std::cout << "[DEBUG] Part 1: '" << parts[1] << "'" << std::endl;
            std::cout << "[DEBUG] Error: " << e.what() << std::endl;
            std::cout << "[DEBUG] Exception type: " << typeid(e).name() << std::endl;
            std::cout << "[DEBUG] ===== END CRASH CONTEXT =====" << std::endl;

            std::string detailed_error = "Failed to parse float interval from value: '" +
                                         std::string(value) +
                                         "'. Cannot convert parts to float. Error: " + e.what();
            throw std::runtime_error(detailed_error);
        }
    }

    throw std::invalid_argument(
        fmt::format("Input value:'{}' does not have the right format: xx-xx.", value));
}

DoubleInterval parse_double_interval(const std::string_view &value, const std::string_view delims) {
    auto parts = split_string(value, delims);
    if (parts.size() == 2) {
        try {
            double start = std::stod(parts[0].data());
            double end = std::stod(parts[1].data());
            return DoubleInterval(start, end);
        } catch (const std::exception &e) {
            std::cout << "\n[DEBUG] ===== CRASH DETECTED IN INTERVAL PARSING =====" << std::endl;
            std::cout << "[DEBUG] Function: parse_double_interval" << std::endl;
            std::cout << "[DEBUG] Input value: '" << value << "'" << std::endl;
            std::cout << "[DEBUG] Delimiters: '" << delims << "'" << std::endl;
            std::cout << "[DEBUG] Split parts: [" << parts.size() << "]" << std::endl;
            std::cout << "[DEBUG] Part 0: '" << parts[0] << "'" << std::endl;
            std::cout << "[DEBUG] Part 1: '" << parts[1] << "'" << std::endl;
            std::cout << "[DEBUG] Error: " << e.what() << std::endl;
            std::cout << "[DEBUG] Exception type: " << typeid(e).name() << std::endl;
            std::cout << "[DEBUG] ===== END CRASH CONTEXT =====" << std::endl;

            std::string detailed_error = "Failed to parse double interval from value: '" +
                                         std::string(value) +
                                         "'. Cannot convert parts to double. Error: " + e.what();
            throw std::runtime_error(detailed_error);
        }
    }

    throw std::invalid_argument(
        fmt::format("Input value:'{}' does not have the right format: xx-xx.", value));
}
} // namespace hgps::core
