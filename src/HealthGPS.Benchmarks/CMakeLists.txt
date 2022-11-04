find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(TBB REQUIRED)

add_executable(HealthGPS.Benchmarks "")
target_compile_features(HealthGPS.Tests PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(HealthGPS.Benchmarks
    PRIVATE
    "BenchMain.cpp"
    "string_benchs.h"
    "identifier_benchs.h")

set(ROOT_NAMESPACE hgps::bench)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(HealthGPS.Benchmarks
    PRIVATE
        HealthGPS.Core
        HealthGPS.Datastore
        HealthGPS
        fmt::fmt
        cxxopts::cxxopts
        benchmark::benchmark)
else()
    target_link_libraries(HealthGPS.Benchmarks
    PRIVATE
        HealthGPS.Core
        HealthGPS.Datastore
        HealthGPS
        Threads::Threads
		#TBB::tbb
        fmt::fmt
        cxxopts::cxxopts
        benchmark::benchmark)
endif()