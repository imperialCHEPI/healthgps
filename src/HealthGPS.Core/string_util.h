#pragma once
#include <string>
#include <functional>

namespace hgps {
	namespace core {

		/// @brief Trim leading and trailing occurrences of white-space characters from string.
		std::string trim(std::string value) noexcept;

		std::string to_lower(std::string_view value) noexcept;

		std::string to_upper(std::string_view value) noexcept;

		std::vector<std::string_view> split_string(std::string_view value, std::string_view delims) noexcept;

		struct case_insensitive {
			struct comparator {
				bool operator() (const std::string& left, const std::string& right) const;
			};

			static std::weak_ordering compare(const std::string& left, const std::string& right)  noexcept;

			static bool equals(const std::string_view& left, const std::string_view& right)  noexcept;

			static bool contains(const std::string_view& text, const std::string_view& str)  noexcept;

			static bool contains(const std::vector<std::string>& source, const std::string_view& element)  noexcept;

			static bool starts_with(const std::string_view& text, const std::string_view& str)  noexcept;

			static bool ends_with(const std::string_view& text, const std::string_view& str)  noexcept;

			static int index_of(const std::vector<std::string>& source, const std::string& element) noexcept;
		};
	}
}