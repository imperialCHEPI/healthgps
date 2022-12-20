find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)

#if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
#   find_package(TBB REQUIRED tbb)
#	if (TBB_FOUND)
#		message(STATUS "TBB library version: ${TBB_VERSION}")
#		option(TBB_ENABLE_IPO "Enable Interprocedural Optimization (IPO) during the compilation" OFF)
#	else()
#		message(STATUS "TBB library NOT found.")
#	endif()
#endif()

include(CTest)
include(GoogleTest)

add_executable(HealthGPS.Tests "")
target_compile_features(HealthGPS.Tests PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(HealthGPS.Tests
	PRIVATE
        "pch.cpp"
        "pch.h"
        "data_config.h"
        "data_config.cpp"
        "AgeGenderTable.Test.cpp"
        "Channel.Test.cpp"
        "Core.Array2DTest.cpp"
        "Core.Test.cpp"
        "Core.UnivariateSummary.Test.cpp"
        "Core.MathHelper.Test.cpp"
        "Datastore.Test.cpp"
        "Repository.Test.cpp"
        "DiseaseModels.Test.cpp"
        "EventAggregator.Test.cpp"
        "HealthGPS.Test.cpp"
        "HierarchicalMapping.Test.cpp"
        "Identifier.Test.cpp"
        "LifeTable.Test.cpp"
        "Map2d.Test.cpp"
        "Population.Test.cpp"
        "RelativeRiskLookup.Test.cpp"
        "RuntimeMetric.Test.cpp"
        "Scenario.Test.cpp"
        "TestMain.cpp"
        "ThreadsafeQueue.Test.cpp"
        "WeightModel.Test.cpp"
        "CountryModule.h"
        "RiskFactorData.h")

set(ROOT_NAMESPACE hgps::test)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_link_libraries(HealthGPS.Tests
    PRIVATE
        HealthGPS.Core
        HealthGPS.Datastore
        HealthGPS
        fmt::fmt
        Threads::Threads
        cxxopts::cxxopts
        GTest::gtest "-ltbb")
else()
    target_link_libraries(HealthGPS.Tests
    PRIVATE
        HealthGPS.Core
        HealthGPS.Datastore
        HealthGPS
        fmt::fmt
        Threads::Threads
        cxxopts::cxxopts
        GTest::gtest)
endif()

gtest_discover_tests(HealthGPS.Tests)
