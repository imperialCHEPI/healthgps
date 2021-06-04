#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>

#include "repository.h"

namespace hgps {
	namespace data {

		class FileRepository :
			public Repository
		{
		public:
			explicit FileRepository(const std::filesystem::path root_directory);
			FileRepository() = delete;

			std::vector<Country> get_countries();

		private:
			const std::filesystem::path root_;
			nlohmann::json index_;
		};
	}
}
