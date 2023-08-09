find_package(fmt REQUIRED)

add_library(HealthGPS.Core STATIC "")
target_compile_features(HealthGPS.Core PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_sources(HealthGPS.Core
	PRIVATE
		"datatable.cpp"
		"datatable.h"
		"interval.cpp"
		"interval.h"
		"string_util.cpp"
		"string_util.h"
		"univariate_summary.cpp"
		"univariate_summary.h"
		"math_util.cpp"
		"math_util.h"
		"identifier.cpp"
		"identifier.h"

		"analysis.h"
		"api.h"
		"array2d.h"
		"column.h"
		"column_builder.h"
		"column_iterator.h"
		"column_numeric.h"
		"column_primitive.h"
		"country.h"
		"datastore.h"
		"disease.h"
		"forward_type.h"
		"indicator.h"
		"mortality.h"
		"poco.h"
		"population_item.h"
		"scoped_timer.h"
		"thread_util.h"
		"visitor.h")

target_link_libraries(HealthGPS.Core
	PRIVATE
		fmt::fmt)

set(ROOT_NAMESPACE hgps::core)