#include "identifier.h"
#include "string_util.h"

#include <algorithm>
#include <fmt/format.h>
#include <stdexcept>

namespace hgps {
namespace core {
Identifier Identifier::empty() {
    static Identifier instance = Identifier();
    return instance;
}

Identifier::Identifier(const std::string &value) : value_{to_lower(value)} {
    if (!value_.empty()) {
        // Trim leading and trailing whitespace
        value_.erase(0, value_.find_first_not_of(" \t\n\r\f\v"));
        value_.erase(value_.find_last_not_of(" \t\n\r\f\v") + 1);

        try {
            validate_identifier();
        } catch (const std::invalid_argument &e) {
            fmt::print("ERROR: Invalid identifier '{}': {}\n", value, e.what());
            throw;
        }
    }

    hash_code_ = std::hash<std::string>{}(value_);
}

Identifier::Identifier(const char *const value) : Identifier{std::string(value)} {}

bool Identifier::is_empty() const noexcept { return value_.empty(); }

std::size_t Identifier::size() const noexcept { return value_.size(); }

const std::string &Identifier::to_string() const noexcept { return value_; }

std::size_t Identifier::hash() const noexcept { return hash_code_; }

bool Identifier::operator==(const Identifier &rhs) const noexcept {
    return hash_code_ == rhs.hash_code_;
}

bool Identifier::equal(const std::string &other) const noexcept {
    return value_.size() == other.size() && value_ == to_lower(other);
}

bool Identifier::equal(const Identifier &other) const noexcept {
    return hash_code_ == other.hash_code_;
}

void Identifier::validate_identifier() const {
    if (std::isdigit(value_.at(0))) {
        throw std::invalid_argument("Identifier must not start with a numeric value");
    }

    if (!std::all_of(std::begin(value_), std::end(value_),
                     [](char c) { return std::isalpha(c) || std::isdigit(c) || c == '_'; })) {
        throw std::invalid_argument("Identifier must contain only valid characters: [a-z_0-9]");
    }
}

std::ostream &operator<<(std::ostream &stream, const Identifier &identifier) {
    stream << identifier.to_string();
    return stream;
}

void from_json(const nlohmann::json &j, Identifier &id) {
    std::string value = j.get<std::string>();

    // Convert to lowercase before creating identifier
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    // Trim leading and trailing whitespace
    value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));
    value.erase(value.find_last_not_of(" \t\n\r\f\v") + 1);

    id = Identifier{value};
}

} // namespace core

core::Identifier operator""_id(const char *id, size_t /*unused*/) { return {id}; }
} // namespace hgps
