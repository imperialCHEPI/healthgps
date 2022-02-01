find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(indicators CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(RAPIDCSV_INCLUDE_DIRS "rapidcsv.h")
find_package(Threads REQUIRED)

add_executable(HealthGPS.Console "")
target_compile_features(HealthGPS.Console PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

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

target_link_libraries(HealthGPS.Console
	PRIVATE
		fmt::fmt
		cxxopts::cxxopts
		indicators::indicators
		nlohmann_json::nlohmann_json
		Threads::Threads
		HealthGPS.Core
		HealthGPS.Datastore
		HealthGPS)

target_include_directories(HealthGPS.Console
	PRIVATE
		${RAPIDCSV_INCLUDE_DIRS})

set(ROOT_NAMESPACE hgps)