#pragma once
#include <functional>
#include <map>
#include <ostream>
#include <string>
#include <unordered_map>

#include "nlohmann/json.hpp"

namespace hgps {
namespace core {

/// @brief Entity unique identifier data type
///
/// @details Wrappers a string identifier to provide consistent
/// comparison operation using hash code instead of strings.
///
/// @note Identifies must not start with a numeric value, and contain
/// only characters: @c _, <tt>alpha: a-z</tt> and <tt>digits: 0-9</tt>.
struct Identifier final {
    /// @brief Initialises a new instance of the hgps::core::Identifier class
    constexpr Identifier() = default;

    /// @brief Initialises a new instance of the hgps::core::Identifier class
    /// @param value The characters array identifier
    /// @exception std::invalid_argument for value starting with a number or containing invalid
    /// characters.
    Identifier(const char *const value);

    /// @brief Initialises a new instance of the hgps::core::Identifier class
    /// @param value The string identifier
    /// @throws std::invalid_argument for value starting with a number or containing invalid
    /// characters.
    Identifier(const std::string &value);

    /// @brief Checks whether this is an empty identifier
    /// @return true if the identifier is empty; otherwise, false.
    bool is_empty() const noexcept;

    /// @brief Gets the side of the identifier
    /// @return The identifier size
    std::size_t size() const noexcept;

    /// @brief Convert the identifier to a string representation
    /// @return The equivalent string representation
    const std::string &to_string() const noexcept;

    /// @brief Gets the identifier hash code value
    /// @return The hash code value
    std::size_t hash() const noexcept;

    /// @brief Determines whether a string representation and this instance have same value.
    /// @param other The string representation to compare to this instance.
    /// @return true if the values are the same; otherwise, false.
    bool equal(const std::string &other) const noexcept;

    /// @brief Determines whether two identifiers have the same value.
    /// @param other The Identifier to compare to this instance.
    /// @return true if the identifiers value are the same; otherwise, false.
    bool equal(const Identifier &other) const noexcept;

    /// @brief Determines whether two identifiers have the same value.
    /// @param rhs The Identifier to compare to this instance.
    /// @return true if the identifiers value are the same; otherwise, false.
    bool operator==(const Identifier &rhs) const noexcept;

    /// @brief Compare two identifier
    /// @param rhs The Identifier to compare to this instance.
    /// @return The comparison result
    std::strong_ordering operator<=>(const Identifier &rhs) const noexcept = default;

    /// @brief Represents the empty Identifier, read-only.
    /// @return The empty identifier
    static Identifier empty();

    /// @brief Output streams operator for Identifier type.
    /// @param stream The stream to output
    /// @param identifier The Identifier instance
    /// @return The output stream
    friend std::ostream &operator<<(std::ostream &stream, const Identifier &identifier);

  private:
    std::string value_{};
    std::size_t hash_code_{std::hash<std::string>{}("")};

    void validate_identifier() const;
};

void from_json(const nlohmann::json &j, Identifier &id);

namespace detail {
template <template <typename...> class Map, class Value>
void map_from_json(const nlohmann::json &j, Map<hgps::core::Identifier, Value> &map) {
    map.clear();
    for (const auto &item : j.items()) {
        map.emplace(item.key(), item.value().get<Value>());
    }
}
} // namespace detail

} // namespace core

//! Allow users to use _id suffix to create Identifier (e.g. "Disease"_id)
core::Identifier operator""_id(const char *id, size_t);
} // namespace hgps

namespace std {
template <class T>
void from_json(const nlohmann::json &j, std::map<hgps::core::Identifier, T> &map) {
    hgps::core::detail::map_from_json(j, map);
}

template <class T>
void from_json(const nlohmann::json &j, std::unordered_map<hgps::core::Identifier, T> &map) {
    hgps::core::detail::map_from_json(j, map);
}

/// @brief Hash code function for Identifier type to be used in unordered containers
template <> struct hash<hgps::core::Identifier> {
    size_t operator()(const hgps::core::Identifier &id) const noexcept { return id.hash(); }
};

} // namespace std
