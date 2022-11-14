find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(indicators CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(RAPIDCSV_INCLUDE_DIRS "rapidcsv.h")
find_package(Threads REQUIRED)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	find_package(TBB REQUIRED)
endif()

add_executable(HealthGPS.Console "")
target_compile_features(HealthGPS.Console PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

configure_file("version.h.in" "version.h" ESCAPE_QUOTES)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

target_sources(HealthGPS.Console
	PRIVATE
		"configuration.cpp"
		"configuration.h"
		"csvparser.cpp"
		"csvparser.h"
		"event_monitor.cpp"
		"event_monitor.h"
		"model_parser.cpp"
		"model_parser.h"
		"program.cpp"
		"result_file_writer.cpp"
		"result_file_writer.h"
		"jsonparser.cpp"
		"jsonparser.h"
		"model_info.h"
		"options.h"
		"resource.h"
		"result_writer.h"
		"riskmodel.h")

# Under Windows, we also include a resource file to the build
if(WIN32)
    # Make sure that the resource file is seen as an RC file to be compiled with a resource compiler, not a C++ compiler
    set_source_files_properties(versioninfo.rc LANGUAGE RC)
	target_sources(HealthGPS.Console PRIVATE versioninfo.rc)
endif(WIN32)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_link_libraries(HealthGPS.Console
	PRIVATE
		HealthGPS.Core
		HealthGPS.Datastore
		HealthGPS
		fmt::fmt
		Threads::Threads
		cxxopts::cxxopts
		indicators::indicators
		nlohmann_json::nlohmann_json
		TBB::tbb "-ltbb")
else()
	target_link_libraries(HealthGPS.Console
	PRIVATE
		HealthGPS.Core
		HealthGPS.Datastore
		HealthGPS
		fmt::fmt
		Threads::Threads
		cxxopts::cxxopts
		indicators::indicators
		nlohmann_json::nlohmann_json)
endif()

target_include_directories(HealthGPS.Console
	PRIVATE
		${RAPIDCSV_INCLUDE_DIRS})

set(ROOT_NAMESPACE hgps)