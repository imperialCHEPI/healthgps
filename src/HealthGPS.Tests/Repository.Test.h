#pragma once

#include <gtest/gtest.h>
#include "HealthGPS/repository.h"
#include "HealthGPS.Core/datastore.h"
#include "mock_repository.h"

namespace hgps {
namespace testing {

// Base test fixture class
class RepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Declare test classes without implementation
class RepositoryTest_CreateRepository_Test : public RepositoryTest {
protected:
    void TestBody() override;  // Declaration only
};

class RepositoryTest_DiseaseInfoIsCached_Test : public RepositoryTest {
protected:
    void TestBody() override;  // Declaration only
};

class RepositoryTest_DiseaseInfoIsCaseInsensitive_Test : public RepositoryTest {
protected:
    void TestBody() override;  // Declaration only
};

} // namespace testing
} // namespace hgps 