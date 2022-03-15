#include(CTest)

# Include sub-projects.
add_subdirectory (HealthGPS.Core)
add_subdirectory (HealthGPS.Datastore)
add_subdirectory (HealthGPS)
add_subdirectory (HealthGPS.Console)

if (BUILD_TESTING)
	enable_testing()
	message(STATUS "Building Tests ...")
	add_subdirectory (HealthGPS.Tests)
endif()

# PUBLIC needed to make heathers and library available to other project
target_include_directories(HealthGPS.Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(HealthGPS.Datastore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(HealthGPS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# External include directory
target_include_directories(HealthGPS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external)

# set the startup project for the "play" button in MSVC
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT HealthGPS.Console)