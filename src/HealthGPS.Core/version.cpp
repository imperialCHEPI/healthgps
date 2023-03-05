#include "version.h"
#include <sstream>
#include <set>

namespace hgps::core {

	int Version::GetMajor()
	{
		return int(API_MAJOR);
	}

	int Version::GetMinor()
	{
		return int(API_MINOR);
	}

	int Version::GetPatch()
	{
		return int(API_PATCH);
	}

	std::string Version::GetVersion()
	{
		static std::string version("");

		if (version.empty())
		{
			// Cache the version string
			std::ostringstream stream;
			stream << API_MAJOR << "." << API_MINOR << "." << API_PATCH;

			version = stream.str();
		}

		return version;
	}

	bool Version::IsAtLeast(int major, int minor, int patch)
	{
		if (API_MAJOR < major) return false;
		if (API_MAJOR > major) return true;
		if (API_MINOR < minor) return false;
		if (API_MINOR > minor) return true;
		if (API_PATCH < patch) return false;
		return true;
	}

	bool Version::HasFeature(const std::string& name)
	{
		static std::set<std::string> features;

		if (features.empty())
		{
			// Cache the feature list
			features.insert("COUNTRY");
			features.insert("THREADSAFE");
		}

		return features.find(name) != features.end();
	}
}
