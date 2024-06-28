find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_path(RAPIDCSV_INCLUDE_DIRS "rapidcsv.h")
find_package(Threads REQUIRED)

add_executable(HealthGPS.Console "program.cpp")
add_library(
    HealthGPS.LibConsole STATIC
    "command_options.cpp"
    "command_options.h"
    "configuration.cpp"
    "configuration.h"
    "configuration_parsing.cpp"
    "configuration_parsing.h"
    "configuration_parsing_helpers.h"
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
    "resource.h"
    "result_writer.h"
    "riskmodel.h"
    "poco.h")

# Under Windows, we also include a resource file to the build
if(WIN32)
    # Make sure that the resource file is seen as an RC file to be compiled with a resource
    # compiler, not a C++ compiler
    set_source_files_properties(versioninfo.rc LANGUAGE RC)
    target_sources(HealthGPS.Console PRIVATE versioninfo.rc)
endif(WIN32)

target_include_directories(HealthGPS.LibConsole PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(
    HealthGPS.Console
    PRIVATE HealthGPS.Core
            HealthGPS.Input
            HealthGPS.LibConsole
            HealthGPS
            fmt::fmt
            Threads::Threads
            cxxopts::cxxopts
            nlohmann_json::nlohmann_json
            TBB::tbb)

target_link_libraries(
    HealthGPS.LibConsole
    PRIVATE HealthGPS.Core
            HealthGPS.Input
            HealthGPS
            fmt::fmt
            Threads::Threads
            cxxopts::cxxopts
            nlohmann_json::nlohmann_json
            TBB::tbb)

install(TARGETS HealthGPS.Console DESTINATION "")
if(WIN32)
    install(IMPORTED_RUNTIME_ARTIFACTS fmt::fmt)
endif()

install(DIRECTORY ../../schemas DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

set(ROOT_NAMESPACE hgps)
