#ifndef HEALTHGPS_INTERFACES_H
#define HEALTHGPS_INTERFACES_H

#include "visibility.h"

namespace hgps {

	// Silly workaround for https://github.com/michaeljones/breathe/issues/453
	constexpr char kDefaultEscapeChar = '\\';

	struct MODEL_API_EXPORT ParseOptions {
		// Parsing options

		/// Field delimiter
		char delimiter = ',';

		/// Whether quoting is used
		bool quoting = true;

		/// Quoting character (if `quoting` is true)
		char quote_char = '"';

		/// Whether a quote inside a value is double-quoted
		bool double_quote = true;

		/// Whether escaping is used
		bool escaping = false;

		/// Escaping character (if `escaping` is true)
		char escape_char = kDefaultEscapeChar;

		/// Whether values are allowed to contain CR (0x0d) and LF (0x0a) characters
		bool newlines_in_values = false;

		/// Whether empty lines are ignored.  If false, an empty line represents
		/// a single empty value (assuming a one-column CSV file).
		bool ignore_empty_lines = true;

		/// Create parsing options with default values
		static ParseOptions Defaults();
	};
}

#endif // HEALTHGPS_INTERFACES_H