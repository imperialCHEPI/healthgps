#pragma once

#include <string>

// Basic std::string utilities that should been part of the language :-)

static bool iequals(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(), b.begin(), b.end(),
		[](char a, char b) { return tolower(a) == tolower(b); });
}

static std::string to_lower(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}

std::string trim(std::string str)
{
    while (!str.empty() && std::isspace(str.back())) str.pop_back();

    std::size_t pos = 0;
    while (pos < str.size() && std::isspace(str[pos])) ++pos;
    return str.substr(pos);
}