#pragma once
#include <string>
#include <ostream>
#include <functional>

namespace hgps::core {
	struct Identifier final
	{
		Identifier() = default;

		explicit Identifier(std::string value);

		bool is_empty() const noexcept;

		std::size_t size() const noexcept;

		std::string to_string() const noexcept;

		std::size_t hash() const noexcept;

		auto operator<=>(const Identifier&) const noexcept = default;

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
