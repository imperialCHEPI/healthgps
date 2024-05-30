find_package(fmt CONFIG REQUIRED)
find_package(jsoncons CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)

add_library(HealthGPS.Datastore STATIC "")
target_compile_features(HealthGPS.Datastore PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(
    HealthGPS.Datastore
    PRIVATE "api.h"
            "datamanager.cpp"
            "datamanager.h"
            "schema.cpp"
            "schema.h"
            "zip_file.cpp"
            "zip_file.h")

target_link_libraries(HealthGPS.Datastore PRIVATE HealthGPS.Core fmt::fmt jsoncons
                                                  libzippp::libzippp)

set(ROOT_NAMESPACE hgps::data)
