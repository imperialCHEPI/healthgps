find_package(fmt CONFIG REQUIRED)

add_library(HealthGPS.Datastore STATIC "")
target_compile_features(HealthGPS.Datastore PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(HealthGPS.Datastore
	PRIVATE
		"datamanager.cpp"
		"datamanager.h"
		"api.h"
)

set(ROOT_NAMESPACE hgps::data)

target_link_libraries(HealthGPS.Datastore
	PRIVATE
		HealthGPS.Core
		fmt::fmt)

# PUBLIC needed to make heathers and library available to other project
target_include_directories(HealthGPS.Datastore PUBLIC ${PROJECT_SOURCE_DIR})
