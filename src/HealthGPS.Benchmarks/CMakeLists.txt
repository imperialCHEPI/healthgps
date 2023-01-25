find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(TBB QUIET)

add_executable(HealthGPS.Benchmarks "")
target_compile_features(HealthGPS.Tests PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(HealthGPS.Benchmarks
    PRIVATE
    "BenchMain.cpp"
    "string_benchs.h"
    "identifier_benchs.h")

if(TBB_FOUND AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    target_link_libraries(HealthGPS.Benchmarks
    PRIVATE
        HealthGPS.Core
        HealthGPS.Datastore
        HealthGPS
        fmt::fmt
        Threads::Threads
        cxxopts::cxxopts
        benchmark::benchmark "-ltbb")
else()
    target_link_libraries(HealthGPS.Benchmarks
    PRIVATE
        HealthGPS.Core
        HealthGPS.Datastore
        HealthGPS
        fmt::fmt
        Threads::Threads
        cxxopts::cxxopts
        benchmark::benchmark)
endif()

install(TARGETS HealthGPS.Benchmarks DESTINATION "")

set(ROOT_NAMESPACE hgps::bench)
