find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
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


add_executable(HealthGPS.Benchmarks "")
target_compile_features(HealthGPS.Tests PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(HealthGPS.Benchmarks
    PRIVATE
    "BenchMain.cpp"
    "string_benchs.h"
    "identifier_benchs.h")

set(ROOT_NAMESPACE hgps::bench)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
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