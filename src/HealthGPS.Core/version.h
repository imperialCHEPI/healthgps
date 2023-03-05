#pragma once

#include <string>

namespace hgps::core {

	/// @brief Version major number
	constexpr auto API_MAJOR = 1;

	/// @brief Version minor number
	constexpr auto API_MINOR = 2;

	/// @brief Version patch number
	constexpr auto API_PATCH = 0;

	/// @brief Application Programming Interface (API) version data type
	class Version
	{
	public:
		Version() = delete;
		Version(const Version&) = delete;
		Version& operator=(const Version&) = delete;
		Version(const Version&&) = delete;
		Version& operator=(const Version&&) = delete;

		/// @brief Gets the API major version
		/// @return The major version value
		static int GetMajor();

		/// @brief Gets the API minor version
		/// @return The minor version value
		static int GetMinor();

		/// @brief Gets the API patch version
		/// @return The patch version value
		static int GetPatch();

		/// @brief Creates a string representation of API version
		/// @return The version string
		static std::string GetVersion();

		/// @brief Validates the API version compatibility
		/// @param major Minimum required major version
		/// @param minor Minimum required minor version
		/// @param patch Minimum required patch version
		/// @return true if the versions are compatible, otherwise false
		static bool IsAtLeast(int major, int minor, int patch);

		/// @brief Checks whether the API version has specific features
		/// @param name The required feature name
		/// @return true if the feature is available, otherwise false
		static bool HasFeature(const std::string& name);
	};
}
