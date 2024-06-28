find_package(fmt CONFIG REQUIRED)
find_package(jsoncons CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(unofficial-curlpp CONFIG REQUIRED)

add_library(HealthGPS.Input STATIC "")
target_compile_features(HealthGPS.Input PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(
    HealthGPS.Input
    PRIVATE "api.h"
            "datamanager.cpp"
            "datamanager.h"
            "download_file.cpp"
            "download_file.h"
            "schema.cpp"
            "schema.h"
            "zip_file.cpp"
            "zip_file.h")

target_link_libraries(HealthGPS.Input PRIVATE HealthGPS.Core fmt::fmt jsoncons libzippp::libzippp
                                              unofficial::curlpp::curlpp)

set(ROOT_NAMESPACE hgps::input)
