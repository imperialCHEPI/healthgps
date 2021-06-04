#include "datamanager.h"

namespace hgps {
	namespace data {
		DataManager::DataManager(Repository& provider) 
			: source_{provider}
		{
		}

		DataManager::~DataManager()
		{
		}

		std::vector<Country> DataManager::get_countries()
		{
			return source_.get_countries();
		}
	}
}