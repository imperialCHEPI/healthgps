cmake_minimum_required (VERSION 3.20)

# minimum default standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(HealthGPS
        VERSION 1.0.0.2
        DESCRIPTION "Global Health Policy Simulation model (Health-GPS)"
        HOMEPAGE_URL "https://github.com/imperialCHEPI/healthgps"
        LANGUAGES CXX)

include(cmake/PreventInSourceBuilds.cmake)

# Adding the tests:
option(ENABLE_TESTING "Enable the tests" ON)

# Generate compile_commands.json to make it easier to work with clang based tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable compiler warnings and standard conformance
if(MSVC)
    add_compile_options(/W4 /permissive-) # /WX 
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-variable) # -Werror
endif()

add_subdirectory (src)