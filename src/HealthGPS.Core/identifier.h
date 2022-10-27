#pragma once
#include <string>
#include <ostream>
#include <functional>

namespace hgps::core {
	struct Identifier final
	{
		constexpr Identifier() = default;

		Identifier(const char* const value);

		Identifier(std::string value);

		bool is_empty() const noexcept;

		std::size_t size() const noexcept;

		std::string to_string() const noexcept;

		std::size_t hash() const noexcept;

		bool equal(const std::string& other) const noexcept;

		bool operator==(const Identifier& rhs) const noexcept;

		std::strong_ordering operator<=>(const Identifier& rhs) const noexcept = default;

		static Identifier empty();

		friend std::ostream& operator<<(std::ostream& stream, const Identifier& identifier);

	private:
		std::string value_{};

		void validate_identifeir() const;
	};
}

template<>
struct std::hash<hgps::core::Identifier>
{
	std::size_t operator()(const hgps::core::Identifier& id) const noexcept { return id.hash(); }
};
