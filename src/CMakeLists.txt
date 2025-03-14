# Workaround for vcpkg issue: https://github.com/microsoft/vcpkg/issues/31617
add_compile_definitions(TBB_USE_ASSERT)

# Include sub-projects.
add_subdirectory(HealthGPS.Core)
add_subdirectory(HealthGPS.Input)
add_subdirectory(HealthGPS)
add_subdirectory(HealthGPS.Console)

if(BUILD_TESTING)
    enable_testing()
    message(STATUS "Building Tests ...")
    add_subdirectory(HealthGPS.Tests)
endif()

if(BUILD_DOC)
    message(STATUS "Doxygen build folder: ${CMAKE_CURRENT_BINARY_DIR}")
    set(DOXYGEN_FILE_PATTERNS *.h *.cpp *.md)
    set(DOXYGEN_EXCLUDE_PATTERNS "*/vcpkg_installed/*" "*/_deps/" "*/docs/*" "*/external/*"
                                 "*/HealthGPS.Tests/*")
    enable_doxygen("awesome-sidebar")
endif()

# PUBLIC needed to make heathers and library available to other project
target_include_directories(HealthGPS.Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(HealthGPS.Input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(HealthGPS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Add binary dir for version.h
target_include_directories(HealthGPS.Core PUBLIC ${CMAKE_BINARY_DIR}/src/HealthGPS)

# External include directory
target_include_directories(HealthGPS.Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external)
target_include_directories(HealthGPS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external)

# Set the start-up project for the "play" button in MSVC
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT HealthGPS.Console)
