#include "pch.h"
#include "data_config.h"

#include "HealthGPS/repository.h"
#include "HealthGPS.Datastore/api.h"
#include "HealthGPS.Core/string_util.h"

#include <chrono>

namespace fs = std::filesystem;

static auto store_full_path = default_datastore_path();

// The fixture for testing class Foo.
class RepositoryTest : public ::testing::Test {
protected:
	RepositoryTest() 
		: manager_{ store_full_path }, repository{ manager_ } {}

	hgps::data::DataManager manager_;
	hgps::CachedRepository repository;
};

TEST_F(RepositoryTest, CreateRepository)
{
	auto& diseases = repository.get_diseases();
	ASSERT_GT(diseases.size(), 0);
}

TEST_F(RepositoryTest, DiseaseInfoIsCached)
{
	using namespace std::chrono;

	auto start = steady_clock::now();
	auto& diseases_cold = repository.get_diseases();
	auto stop = steady_clock::now();
	auto duration_cold = (stop - start);

	auto number_of_trials = 13;
	for (auto i = 0; i < number_of_trials; i++) {
		start = steady_clock::now();
		auto& diseases_hot = repository.get_diseases();
		stop = steady_clock::now();
		auto duration_hot = (stop - start);
		
		ASSERT_EQ(diseases_cold.size(), diseases_hot.size());
		ASSERT_GT(duration_cold.count(), duration_hot.count());
	}
}

TEST_F(RepositoryTest, DiseaseInfoIsCaseInsensitive)
{
	using namespace hgps;
	auto& all_diseases = repository.get_diseases();
	ASSERT_GT(all_diseases.size(), 0);
	auto& disease = all_diseases.at(0);

	auto code_title = disease.code;
	if (std::islower(code_title.at(0))) {
		code_title.at(0) = static_cast<char>(std::toupper(code_title.at(0)));
	}
	else {
		code_title.at(0) = static_cast<char>(std::tolower(code_title.at(0)));
	}

	auto info_code = repository.get_disease_info(disease.code);
	auto info_title = repository.get_disease_info(code_title);
	auto info_upper = repository.get_disease_info(core::to_upper(disease.code));
	auto info_lower = repository.get_disease_info(core::to_lower(disease.code));

	ASSERT_TRUE(info_code.has_value());
	ASSERT_TRUE(info_title.has_value());
	ASSERT_TRUE(info_upper.has_value());
	ASSERT_TRUE(info_lower.has_value());
}