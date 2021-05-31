#ifndef HEALTHGPS_CORE_VERSION_H
#define HEALTHGPS_CORE_VERSION_H

#include <string>
#include "visibility.h"

constexpr auto API_MAJOR = 0;
constexpr auto API_MINOR = 1;
constexpr auto API_PATCH = 0;

namespace hgps {
	namespace core {

		class CORE_API_EXPORT Version
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

#endif // HEALTHGPS_CORE_VERSION_H