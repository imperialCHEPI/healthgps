#ifndef HEALTHGPS_VISIBILITY_H
#define HEALTHGPS_VISIBILITY_H

#if defined(_WIN32) || defined(__CYGWIN__)
// Windows System

	#if defined(_MSC_VER)
		#pragma warning(disable : 4251)
	#else
		#pragma GCC diagnostic ignored "-Wattributes"
	#endif

	#ifdef MODEL_API_STATIC // Static linked library (.lib)
		#define MODEL_API_EXPORT
	#elif defined(MODEL_API_EXPORTING) // Dynamic linked library (.dll)
		#define MODEL_API_EXPORT __declspec(dllexport)
	#else
		#define MODEL_API_EXPORT __declspec(dllimport)
	#endif

	#define MODEL_API_HIDDEN

#elif defined(__GNUC__) || defined(__clang__)
// Not Windows System

	#ifdef MODEL_API_STATIC // Static library (.o)
		#define MODEL_API_EXPORT
		#define MODEL_API_HIDDEN
	#elif defined(MODEL_API_EXPORTING) // Shared library (.so)
		#define MODEL_API_EXPORT __attribute__ ((visibility("default")))
		#define MODEL_API_HIDDEN __attribute__ ((visibility("hidden")))
	#else
		// If you use -fvisibility=hidden in GCC, exception handling and RTTI
		// would break if visibility wasn't set during export _and_ import
		// because GCC would immediately forget all type infos encountered.
		// See http://gcc.gnu.org/wiki/Visibility
		#define MODEL_API_EXPORT __attribute__ ((visibility ("default")))
		#define MODEL_API_HIDDEN
	#endif
#else
	#error Unknown compiler, please implement shared library macros
#endif  // Non-Windows

#endif // HEALTHGPS_VISIBILITY_H