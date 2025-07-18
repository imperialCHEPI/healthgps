find_package(fmt CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_path(RAPIDCSV_INCLUDE_DIRS "rapidcsv.h")
find_package(Threads REQUIRED)

add_executable(HealthGPS.Console "program.cpp")
add_library(
    HealthGPS.LibConsole STATIC
    "command_options.cpp"
    "command_options.h"
    "event_monitor.cpp"
    "event_monitor.h"
    "program.cpp"
    "result_file_writer.cpp"
    "result_file_writer.h"
    "model_info.h"
    "resource.h"
    "result_writer.h")

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
            TBB::tbb)

target_link_libraries(
    HealthGPS.LibConsole
    PRIVATE HealthGPS.Core
            HealthGPS.Input
            HealthGPS
            fmt::fmt
            Threads::Threads
            cxxopts::cxxopts
            TBB::tbb)

install(
    TARGETS HealthGPS.Console
    RUNTIME
    ARCHIVE
    LIBRARY
    RUNTIME
    FRAMEWORK
    BUNDLE
    PUBLIC_HEADER
    RESOURCE)
if(WIN32)
    install(
        TARGETS HealthGPS.Console
        COMPONENT HealthGPS.Console
        RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES
        "api-ms-"
        "ext-ms-"
        POST_EXCLUDE_REGEXES
        ".*system32/.*\\.dll"
        DIRECTORIES
        $<TARGET_FILE_DIR:HealthGPS.Console>)
endif()

install(DIRECTORY ../../schemas/config DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/schemas")

set(ROOT_NAMESPACE hgps)
