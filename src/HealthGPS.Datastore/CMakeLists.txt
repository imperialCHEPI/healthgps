find_package(fmt CONFIG REQUIRED)
find_package(jsoncons CONFIG REQUIRED)

add_library(HealthGPS.Datastore STATIC "")
target_compile_features(HealthGPS.Datastore PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(HealthGPS.Datastore PRIVATE "datamanager.cpp" "datamanager.h" "api.h")

target_link_libraries(HealthGPS.Datastore PRIVATE HealthGPS.Core fmt::fmt jsoncons)

set(ROOT_NAMESPACE hgps::data)
