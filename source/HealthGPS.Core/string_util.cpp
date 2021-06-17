#include <algorithm>
#include "string_util.h"

namespace hgps {
    namespace core {

        std::string trim(std::string value) {
            while (!value.empty() && std::isspace(value.back())) value.pop_back();

            std::size_t pos = 0;
            while (pos < value.size() && std::isspace(value[pos])) ++pos;
            return value.substr(pos);
        }

        std::string to_lower(std::string_view value)
        {
            std::string result = std::string(value);
            std::transform(value.begin(), value.end(), result.begin(), ::tolower);
            return result;
        }

        std::string to_upper(std::string_view value)
        {
            std::string result = std::string(value);
            std::transform(value.begin(), value.end(), result.begin(), ::toupper);
            return result;
        }

        bool case_insensitive::comparator::operator()(const std::string& left, const std::string& right) const {
            std::string str_left(left.length(), ' ');
            std::string str_right(right.length(), ' ');

            std::transform(left.begin(), left.end(), str_left.begin(), ::tolower);
            std::transform(right.begin(), right.end(), str_right.begin(), ::tolower);
            return  str_left < str_right;
        }

        bool case_insensitive::equals(const std::string_view& left, const std::string_view& right) {
            if (left.size() != right.size()) {
                return false;
            }

            return std::equal(left.begin(), left.end(), right.begin(), right.end(),
                [](char a, char b) { return tolower(a) == tolower(b); });
        }
    }
}