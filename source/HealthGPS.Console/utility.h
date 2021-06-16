#pragma once

#include <string>

// Basic std::string utilities that should been part of the language :-)

std::string trim(std::string str)
{
    while (!str.empty() && std::isspace(str.back())) str.pop_back();

    std::size_t pos = 0;
    while (pos < str.size() && std::isspace(str[pos])) ++pos;
    return str.substr(pos);
}

struct case_insensitive {
    struct comparator {
        bool operator() (const std::string& s1, const std::string& s2) const {
            std::string str1(s1.length(), ' ');
            std::string str2(s2.length(), ' ');
            std::transform(s1.begin(), s1.end(), str1.begin(), ::tolower);
            std::transform(s2.begin(), s2.end(), str2.begin(), ::tolower);
            return  str1 < str2;
        }
    };

    static bool equals(const std::string& a, const std::string& b)
    {
        if (a.size() != b.size()) { return false; }

        return std::equal(a.begin(), a.end(), b.begin(), b.end(),
            [](char a, char b) { return tolower(a) == tolower(b); });
    }
};

static std::string to_lower(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}