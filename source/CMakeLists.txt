cmake_minimum_required (VERSION 3.20)

# minimum default standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(HealthGPS
        VERSION 1.0
        DESCRIPTION "Global Health Policy Simulation model (Health-GPS)"
        LANGUAGES CXX)

include(cmake/PreventInSourceBuilds.cmake)

# Generate compile_commands.json to make it easier to work with clang based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable compiler warnings and standard conformance
if(MSVC)
    add_compile_options(/W4 /permissive-) # /WX 
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-variable) # -Werror
endif()

include(CTest)

# Include sub-projects.
add_subdirectory (HealthGPS.Core)
add_subdirectory (HealthGPS.Datastore)
add_subdirectory (HealthGPS)
add_subdirectory (HealthGPS.Console)

if (BUILD_TESTING)
    enable_testing()
    add_subdirectory (HealthGPS.Tests)
endif()