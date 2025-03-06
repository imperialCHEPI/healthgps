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

// The fixture for testing class Foo.
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
