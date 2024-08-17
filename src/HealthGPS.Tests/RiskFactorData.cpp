#include "RiskFactorData.h"

template <typename TYPE>
std::string join_string(const std::vector<TYPE> &v, std::string_view delimiter, bool quoted) {
    std::stringstream s;
    if (!v.empty()) {
        std::string_view q = quoted ? "\"" : "";
        s << q << v.front() << q;
        for (size_t i = 1; i < v.size(); ++i) {
            s << delimiter << " " << q << v[i] << q;
        }
    }

    return s.str();
}

std::string join_string_map(const std::vector<std::string> &v, std::string_view delimiter) {
    std::stringstream s;
    if (!v.empty()) {
        std::string_view q = "\"";
        s << "{{" << q << v.front() << q << ",0}";
        for (size_t i = 1; i < v.size(); ++i) {
            s << delimiter << " {" << q << v[i] << q << delimiter << i << "}";
        }

        s << "}";
    }

    return s.str();
}
