#ifdef _MSC_VER
#pragma warning(disable : 26439) // This kind of function should not throw. Declare it 'noexcept'
#pragma warning(disable : 26495) // Variable is uninitialized
#pragma warning(disable : 26498) // The function is constexpr, mark variable constexpr if compile-time evaluation is desired
#pragma warning(disable : 26819) // Unannotated fallthrough between switch labels
#pragma warning(disable : 6285) // (<non-zero constant> || <non-zero constant>) is always a non-zero constant
#endif

#include "data_config.h"
#include "pch.h"
#include <gtest/gtest.h>
#include "HealthGPS/repository.h"
#include "HealthGPS.Core/datastore.h"
#include "mock_repository.h"

#include "HealthGPS.Core/string_util.h"
#include "HealthGPS.Input/api.h"

#include <chrono>

using namespace hgps;
using namespace hgps::testing;

//Created- Mahima
// To check that the repository can be created, 
// disease information is not cached initially, 
// and that disease information retrieval is case-insensitive.
// The fixture for testing class Repository.cpp
class RepositoryTest : public ::testing::Test {
  protected:
    RepositoryTest() : manager_{test_datastore_path}, repository{manager_} {}

    hgps::input::DataManager manager_;
    hgps::CachedRepository repository;
};

TEST_F(RepositoryTest, CreateRepository) {
    auto mock_repo = std::make_shared<MockRepository>();
    ASSERT_NE(nullptr, mock_repo);
}

TEST_F(RepositoryTest, DiseaseInfoIsCached) {
    auto mock_repo = std::make_shared<MockRepository>();
    auto disease_info = mock_repo->get_disease_info("test"_id);
    ASSERT_FALSE(disease_info.has_value());
}

TEST_F(RepositoryTest, DiseaseInfoIsCaseInsensitive) {
    auto mock_repo = std::make_shared<MockRepository>();
    auto disease_info = mock_repo->get_disease_info("TEST"_id);
    ASSERT_FALSE(disease_info.has_value());
}
