#ifndef HEALTHGPS_DATASTORE_INTERFACES_H
#define HEALTHGPS_DATASTORE_INTERFACES_H

#include <cstdint>
#include <vector>
#include <string>

#include "visibility.h"

namespace hgps {
	namespace data {

		struct DATA_API_EXPORT ReadOptions {
			// Reader options

			/// Whether to use the global CPU thread pool
			bool use_threads = true;

			/// Block size we request from the IO layer; also determines the size of
			/// chunks when use_threads is true
			int32_t block_size = 1 << 20;  // 1 MB

			/// Number of header rows to skip (not including the row of column names, if any)
			int32_t skip_rows = 0;

			/// Column names for the target table.
			/// If empty, fall back on autogenerate_column_names.
			std::vector<std::string> column_names;

			/// Whether to autogenerate column names if `column_names` is empty.
			/// If true, column names will be of the form "f0", "f1"...
			/// If false, column names will be read from the first CSV row after `skip_rows`.
			bool autogenerate_column_names = false;

			/// Create read options with default values
			static ReadOptions Defaults();
		};
	}
}

#endif // HEALTHGPS_DATASTORE_INTERFACES_H