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
            "TestConsoleCapture.h"
            "TestConsoleCapture.cpp"
            "data_config.h"
            "data_config.cpp"
            "AgeGenderTable.Test.cpp"
            "Channel.Test.cpp"
            "Configuration.Test.cpp"
            "ConfigSchemaExpanded.Test.cpp"
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
            "JacardiEducation.Test.cpp"
            "JacardiEmployment.Test.cpp"
            "JacardiModel.Test.cpp"
            "CostOfDiseaseSchema.Test.cpp"
            "LifeTable.Test.cpp"
            "Map2d.Test.cpp"
            "Population.Test.cpp"
            "PredictorResolver.Test.cpp"
            "DemographicSummary.Test.cpp"
            "DataSeries.Test.cpp"
            "ModelParserFinch.Test.cpp"
            "KevinHallWeightValidation.Test.cpp"
            "KevinHallHeight.Test.cpp"
            "KevinHallWeightQuantiles.Test.cpp"
            "EventMonitor.Test.cpp"
            "RelativeRiskLookup.Test.cpp"
            "RuntimeMetric.Test.cpp"
            "Scenario.Test.cpp"
            "TestMain.cpp"
            "WeightModel.Test.cpp"
            "CountryModule.h"
            "RiskFactorData.h"
            "Interval.Test.cpp"
            "LoadNutrientTable.cpp"
            "PIFData.Test.cpp"
            "DataManagerPIF.Test.cpp"
            "DiseaseModelPIF.Test.cpp"
            "RepositoryPIF.Test.cpp"
            "ConfigurationPIF.Test.cpp"
            "PIFIntegration.Test.cpp"
            "IncomeStratumAdjustment.Test.cpp"
            "IncomeCategoryLayout.Test.cpp"
            "ConfigLegacyFields.Test.cpp"
            "ResultFileWriter.Test.cpp")

# Download and extract test data
cmake_policy(SET CMP0135 NEW)
include(FetchContent)
FetchContent_Declare(
    test_data
    URL https://github.com/imperialCHEPI/healthgps-data/releases/download/20240624/data_20240624.zip
    URL_HASH SHA256=b68abc8d40b937d377aa7357bff66a1f4f5196da5b867f7ca88d02b37d2ebb5c)
FetchContent_MakeAvailable(test_data)
target_compile_definitions(HealthGPS.Tests PRIVATE "-DTEST_DATA_PATH=\"${test_data_SOURCE_DIR}\"")
# MAHIMA: Employment SCORE fixtures live in healthgps-examples/Jacardi_Slovenia (not duplicated
# under Tests). Override with -DJACARDI_SLOVENIA_DIR=... if the pack is not a sibling checkout.
if(NOT DEFINED JACARDI_SLOVENIA_DIR)
    set(JACARDI_SLOVENIA_DIR "${CMAKE_SOURCE_DIR}/../healthgps-examples/Jacardi_Slovenia")
endif()
set(JACARDI_EMPLOYMENT_FIXTURE_DIR "")
if(EXISTS "${JACARDI_SLOVENIA_DIR}/employment_static_coefs.csv"
   AND EXISTS "${JACARDI_SLOVENIA_DIR}/employment_4cat_recode.csv"
   AND EXISTS "${JACARDI_SLOVENIA_DIR}/employment_static_check.csv")
    set(JACARDI_EMPLOYMENT_FIXTURE_DIR "${JACARDI_SLOVENIA_DIR}")
    message(STATUS "JACARDI employment fixtures: ${JACARDI_EMPLOYMENT_FIXTURE_DIR}")
else()
    message(
        WARNING "JACARDI employment pack not found at ${JACARDI_SLOVENIA_DIR} "
                "(need employment_static_coefs.csv, employment_4cat_recode.csv, "
                "employment_static_check.csv). JacardiEmployment CSV tests will GTEST_SKIP. "
                "Clone healthgps-examples as a sibling or pass -DJACARDI_SLOVENIA_DIR=...")
endif()
target_compile_definitions(
    HealthGPS.Tests
    PRIVATE "-DJACARDI_EMPLOYMENT_FIXTURE_DIR=\"${JACARDI_EMPLOYMENT_FIXTURE_DIR}\"")

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
    install(IMPORTED_RUNTIME_ARTIFACTS fmt::fmt)
    install(IMPORTED_RUNTIME_ARTIFACTS GTest::gtest)
endif()

# Additional data files required for tests
set(TO_COPY example_file.txt nutrient_table.csv ../../schemas)
foreach(FILE IN LISTS TO_COPY)
    file(COPY "${FILE}" DESTINATION ".")
endforeach(FILE IN LISTS TO_COPY)

set(ROOT_NAMESPACE hgps::test)
