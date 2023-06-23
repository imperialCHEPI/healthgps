#pragma once
#include <functional>
#include <string>

namespace hgps::core {

/// @brief Trim leading and trailing occurrences of white-space characters from string.
/// @param value The string to trim
/// @return The resulting string
std::string trim(std::string value) noexcept;

/// @brief Converts the given ASCII string to lower-case
/// @param value The string to convert
/// @return The string in lower-case
std::string to_lower(const std::string_view &value) noexcept;

/// @brief Converts the given ASCII string to upper-case
/// @param value The string to convert
/// @return The string in upper-case
std::string to_upper(const std::string_view &value) noexcept;

/// @brief Splits a string into substrings based on specified delimiting characters.
/// @param value The source string to split
/// @param delims The delimiting character to split
/// @return An array whose elements contain the substrings
std::vector<std::string_view> split_string(const std::string_view &value,
                                           std::string_view delims) noexcept;

/// @brief Case-insensitive operations on ASCII strings.
struct case_insensitive final {

    /// @brief Case-insensitive ASCII strings comparator structure
    struct comparator {
        /// @brief Case-insensitive ASCII strings default comparer
        /// @param left The left string to compare
        /// @param right The right string to compare
        /// @return true if the string are lexicographical equal, otherwise, false
        bool operator()(const std::string_view &left, const std::string_view &right) const;
    };

    /// @brief Weak-ordering case-insensitive ASCII strings comparer
    /// @param left The left string to compare
    /// @param right The right string to compare
    /// @return true if the string are equal, otherwise, false
    static std::weak_ordering compare(const std::string_view &left,
                                      const std::string_view &right) noexcept;

    /// @brief Compare two case-insensitive ASCII string for equality
    /// @param left The left string to compare
    /// @param right The right string to compare
    /// @return true if the string are equal, otherwise, false
    static bool equals(const std::string_view &left, const std::string_view &right) noexcept;

    /// @brief Checks whether a specified substring occurs within a ASCII string
    /// @param text The source string
    /// @param str The string to seek
    /// @return true if the value occurs within the string, otherwise, false.
    static bool contains(const std::string_view &text, const std::string_view &str) noexcept;

    /// @brief Checks whether a vector contains a ASCII strings element
    /// @param source The vector of strings
    /// @param element The string to seek
    /// @return true if the value occurs within the vector, otherwise, false.
    static bool contains(const std::vector<std::string> &source,
                         const std::string_view &element) noexcept;

    /// @brief Determines whether the start of a ASCII string matches a specified string
    /// @param text The source string.
    /// @param str The start string to compare
    /// @return true if value matches the start of string; otherwise, false.
    static bool starts_with(const std::string_view &text, const std::string_view &str) noexcept;

    /// @brief Determines whether the end of a ASCII string matches a specified string
    /// @param text The source string.
    /// @param str The end string to compare
    /// @return true if value matches the end of string; otherwise, false.
    static bool ends_with(const std::string_view &text, const std::string_view &str) noexcept;

    /// @brief Finds the zero-based index of the first occurrence of an element in a vector.
    /// @param source The vector of strings
    /// @param element The element to locate
    /// @return The zero-based index the element first occurrence, if found; otherwise, -1.
    static int index_of(const std::vector<std::string> &source,
                        const std::string_view &element) noexcept;

  private:
    static bool equal_char(char left, char right) noexcept;
};
} // namespace hgps::core