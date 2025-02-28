find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(Threads REQUIRED)

include(CTest)
include(GoogleTest)

add_executable(HealthGPS.Tests "")
target_compile_features(HealthGPS.Tests PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(
    HealthGPS.Tests
    PRIVATE "pch.cpp"
            "pch.h"
            "data_config.h"
            "data_config.cpp"
            "AgeGenderTable.Test.cpp"
            "Channel.Test.cpp"
            "Configuration.Test.cpp"
            "Core.Array2DTest.cpp"
            "Core.Test.cpp"
            "Core.UnivariateSummary.Test.cpp"
            "Core.MathHelper.Test.cpp"
            "Sha256.Test.cpp"
            "Datastore.Test.cpp"
            "Repository.Test.cpp"
            "DiseaseModels.Test.cpp"
            "EventAggregator.Test.cpp"
            "Simulation.Test.cpp"
            "HierarchicalMapping.Test.cpp"
            "JsonParser.Test.cpp"
            "Identifier.Test.cpp"
            "LifeTable.Test.cpp"
            "Map2d.Test.cpp"
            "Population.Test.cpp"
            "RelativeRiskLookup.Test.cpp"
            "RuntimeMetric.Test.cpp"
            "Scenario.Test.cpp"
            "TestMain.cpp"
            "WeightModel.Test.cpp"
            "CountryModule.h"
            "RiskFactorData.h"
            "Interval.Test.cpp"
            "LoadNutrientTable.cpp")

# Download and extract test data
cmake_policy(SET CMP0135 NEW)
include(FetchContent)
FetchContent_Declare(
    test_data
    URL https://github.com/imperialCHEPI/healthgps-data/releases/download/20240624/data_20240624.zip
    URL_HASH SHA256=b68abc8d40b937d377aa7357bff66a1f4f5196da5b867f7ca88d02b37d2ebb5c)
FetchContent_MakeAvailable(test_data)
target_compile_definitions(HealthGPS.Tests PRIVATE "-DTEST_DATA_PATH=\"${test_data_SOURCE_DIR}\"")

target_link_libraries(
    HealthGPS.Tests
    PRIVATE HealthGPS.Core
            HealthGPS.Input
            HealthGPS.LibConsole
            HealthGPS
            fmt::fmt
            Threads::Threads
            cxxopts::cxxopts
            TBB::tbb
            GTest::gtest)

# We use a long discovery timeout because this can take a while on CI systems
gtest_discover_tests(HealthGPS.Tests DISCOVERY_TIMEOUT 30)

install(TARGETS HealthGPS.Tests DESTINATION "")
if(WIN32)
    install(IMPORTED_RUNTIME_ARTIFACTS GTest::gtest)
endif()

# Additional data files required for tests
set(TO_COPY example_file.txt nutrient_table.csv ../../schemas)
foreach(FILE IN LISTS TO_COPY)
    file(COPY "${FILE}" DESTINATION ".")
endforeach(FILE IN LISTS TO_COPY)

set(ROOT_NAMESPACE hgps::test)
