cmake_minimum_required(VERSION 3.20)

option(BUILD_DOC "Build API documentation" OFF)
option(WARNINGS_AS_ERRORS "Make the build fail if the compiler gives a warning" OFF)

# minimum default standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CATCH_EXCEPTIONS)
    # By default, catch all exceptions for release builds only
    string(REGEX MATCH "Rel" RELEASE_MATCH "${CMAKE_BUILD_TYPE}")
    if(NOT RELEASE_MATCH STREQUAL "")
        set(CATCH_EXCEPTIONS TRUE)
    endif()
endif()

# Catch uncaught exceptions in main()
if(CATCH_EXCEPTIONS)
    add_definitions(-DCATCH_EXCEPTIONS)
endif()

project(
    Health-GPS
    VERSION 1.2.2.0
    DESCRIPTION "Global Health Policy Simulation model (Health-GPS)"
    HOMEPAGE_URL "https://github.com/imperialCHEPI/healthgps"
    LANGUAGES CXX)

include(FetchContent)
include(cmake/PreventInSourceBuilds.cmake)
include(cmake/Doxygen.cmake)

# Generate compile_commands.json to make it easier to work with clang based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable compiler warnings and standard conformance
message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
if(MSVC)
    add_compile_options(/W4 /permissive-)

    if(WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)

    if(WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

include(CTest)

add_subdirectory(src)
