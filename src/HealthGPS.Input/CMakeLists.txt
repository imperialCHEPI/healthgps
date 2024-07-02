find_package(fmt CONFIG REQUIRED)
find_package(jsoncons CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(unofficial-curlpp CONFIG REQUIRED)

add_library(HealthGPS.Input STATIC "")
target_compile_features(HealthGPS.Input PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(
    HealthGPS.Input
    PRIVATE "api.h"
            "configuration.cpp"
            "configuration.h"
            "configuration_parsing.cpp"
            "configuration_parsing.h"
            "configuration_parsing_helpers.h"
            "csvparser.cpp"
            "csvparser.h"
            "datamanager.cpp"
            "datamanager.h"
            "data_source.cpp"
            "data_source.h"
            "download_file.cpp"
            "download_file.h"
            "jsonparser.cpp"
            "jsonparser.h"
            "model_parser.cpp"
            "model_parser.h"
            "poco.h"
            "riskmodel.h"
            "schema.cpp"
            "schema.h"
            "zip_file.cpp"
            "zip_file.h")

target_link_libraries(HealthGPS.Input PRIVATE HealthGPS HealthGPS.Core fmt::fmt jsoncons
                                              libzippp::libzippp unofficial::curlpp::curlpp)

set(ROOT_NAMESPACE hgps::input)
