find_package(fmt CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(HealthGPS.Tests "")
target_compile_features(HealthGPS.Tests PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(HealthGPS.Tests
	PRIVATE
        "pch.cpp"
        "pch.h"
        "data_config.h"
        "AgeGenderTable.Test.cpp"
        "Channel.Test.cpp"
        "Core.Array2DTest.cpp"
        "Core.Test.cpp"
        "Core.UnivariateSummary.Test.cpp"
        "Datastore.Test.cpp"
        "DiseaseModels.Test.cpp"
        "EventAggregator.Test.cpp"
        "HealthGPS.Test.cpp"
        "HierarchicalMapping.Test.cpp"
        "LifeTable.Test.cpp"
        "Map2d.Test.cpp"
        "Population.Test.cpp"
        "RelativeRiskLookup.Test.cpp"
        "RuntimeMetric.Test.cpp"
        "Scenario.Test.cpp"
        "TestMain.cpp"
        "ThreadsafeQueue.Test.cpp"
        "CountryModule.h"
        "RiskFactorData.h")

set(ROOT_NAMESPACE hgps::test)

target_link_libraries(HealthGPS.Tests
    PRIVATE
        HealthGPS.Core
        HealthGPS.Datastore
        HealthGPS
        fmt::fmt
        GTest::gtest)

gtest_discover_tests(HealthGPS.Tests)


