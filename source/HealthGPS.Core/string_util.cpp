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

        std::string to_lower(std::string_view value) {
            std::string result = std::string(value);
            std::transform(value.begin(), value.end(), result.begin(), ::tolower);
            return result;
        }

        std::string to_upper(std::string_view value) {
            std::string result = std::string(value);
            std::transform(value.begin(), value.end(), result.begin(), ::toupper);
            return result;
        }

        bool case_insensitive::comparator::operator()(
            const std::string& left, const std::string& right) const {
            return to_lower(left) < to_lower(right);
        }

        std::weak_ordering case_insensitive::compare(
            const std::string& left, const std::string& right) {

            int cmp = to_lower(left).compare(to_lower(right));
            if (cmp < 0) {
                return std::weak_ordering::less;
            }
            else if (cmp > 0) {
                return std::weak_ordering::greater;
            }
            
            return std::weak_ordering::equivalent;
        }

        bool case_insensitive::equals(const std::string_view& left,
            const std::string_view& right) {

            if (left.size() != right.size()) {
                return false;
            }

            return std::equal(left.begin(), left.end(), right.begin(), right.end(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); });
        }

        bool case_insensitive::contains(const std::string_view& text,
            const std::string_view& str) {

            if (str.length() > text.length()) {
                return false;
            }

            auto it = std::search(text.begin(), text.end(), str.begin(), str.end(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); });

            return it != text.end();
        }

        bool case_insensitive::starts_with(const std::string_view& text,
            const std::string_view& str) {

            if (str.length() > text.length()) {
                return false;
            }

            auto it = std::search(text.begin(), text.begin() + str.length(),
                str.begin(), str.end(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); });

            return it == text.begin();
        }

        bool case_insensitive::ends_with(const std::string_view& text,
            const std::string_view& str) {

            if (str.length() > text.length()) {
                return false;
            }

            auto it = std::search(text.rbegin(), text.rbegin() + str.length(),
                str.rbegin(), str.rend(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); });

            return it == text.rbegin();
        }
    }
}