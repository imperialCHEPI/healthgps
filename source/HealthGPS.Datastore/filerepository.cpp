#include <fstream>
#include <rapidcsv.h>

#include "filerepository.h"

namespace hgps {
	namespace data {

		FileRepository::FileRepository(const std::filesystem::path root_directory)
			: root_{root_directory}
		{
			assert(root_.is_absolute());
			assert(std::filesystem::exists(root_));

			std::ifstream ifs(root_ / "index.json", std::ifstream::in);
			if (ifs) {
				index_ = nlohmann::json::parse(ifs);
			}
		}

		std::vector<Country> FileRepository::get_countries()
		{
			auto results = std::vector<Country>();
			if (index_.contains("country")) {
				auto filename = index_["country"]["filename"].get<std::string>();
				filename = (root_ / filename).string();

				// TODO: use precondition contract
				if (std::filesystem::exists(filename)) {
					rapidcsv::Document doc(filename);
					for (size_t i = 0; i < doc.GetRowCount(); i++)
					{
						auto row = doc.GetRow<std::string>(i);
						results.push_back(Country
							{
								.code = row[0],
								.name = row[1]
							});
					}

					std::sort(results.begin(), results.end());
				}
			}

			return results;
		}
	}
}