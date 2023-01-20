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

if (BUILD_BENCHMARKS)
	message(STATUS "Building Benchmarks ...")
	add_subdirectory (HealthGPS.Benchmarks)
endif()

# PUBLIC needed to make heathers and library available to other project
target_include_directories(HealthGPS.Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(HealthGPS.Datastore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(HealthGPS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# External include directory
target_include_directories(HealthGPS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external)

# Set the start-up project for the "play" button in MSVC
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT HealthGPS.Console)
