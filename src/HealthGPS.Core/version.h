#pragma once

#include <string>

namespace hgps {
	namespace core {

		constexpr auto API_MAJOR = 1;
		constexpr auto API_MINOR = 0;
		constexpr auto API_PATCH = 0;

		class Version
		{
		public:
			Version() = delete;
			Version(const Version&) = delete;
			Version& operator=(const Version&) = delete;
			Version(const Version&&) = delete;
			Version& operator=(const Version&&) = delete;

			static int GetMajor();
			static int GetMinor();
			static int GetPatch();
			static std::string GetVersion();
			static bool IsAtLeast(int major, int minor, int patch);
			static bool HasFeature(const std::string& name);
		};
	}
}
