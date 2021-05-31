#include "options.h"
#include "..\HealthGPS.Core\version.h"

namespace hgps {
	namespace data {

		std::string ReadOptions::GetApiVersion() { return hgps::core::Version::GetVersion(); }

		ReadOptions ReadOptions::Defaults() { return ReadOptions(); }
	}
}
 