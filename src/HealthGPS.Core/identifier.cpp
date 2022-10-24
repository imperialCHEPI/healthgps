#include "identifier.h"
#include "string_util.h"

#include <algorithm>
#include <stdexcept>

namespace hgps::core {
	Identifier Identifier::empty() {
		static Identifier instance = Identifier();
		return instance;
	}

	Identifier::Identifier(std::string value)
		: value_{ std::move(to_lower(value)) }
	{
		validate_identifeir();
	}

	bool Identifier::is_empty() const noexcept {
		return value_.empty();
	}

	std::size_t Identifier::size() const noexcept {
		return value_.size();
	}

	std::string Identifier::to_string() const noexcept {
		return value_;
	}

	std::size_t Identifier::hash() const noexcept {
		return std::hash<std::string>{}(value_);
	}

	void Identifier::validate_identifeir() const {
		if (value_.empty() || value_.size() < std::size_t{ 1 }) {
			throw std::invalid_argument("Identifier must not be empty");
		}

		if (std::isdigit(value_.at(0))) {
			throw std::invalid_argument("Identifier must not start with a numeric value");
		}

		if (!std::all_of(std::begin(value_), std::end(value_),
			[](char c) { return std::isalpha(c) || std::isdigit(c) || c == '_'; }))
		{
			throw std::invalid_argument("Identifier must contain only valid characters: [a-z_0-9]");
		}
	}

	std::ostream& operator<<(std::ostream& stream, const Identifier& identifier)
	{
		stream << identifier.to_string();
		return stream;
	}
}