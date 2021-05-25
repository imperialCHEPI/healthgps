#pragma once
#ifndef HEALTHGPS_CORE_VISIBILITY_H
#define HEALTHGPS_CORE_VISIBILITY_H

#if defined(_WIN32) || defined(__CYGWIN__)
// Windows System

	#if defined(_MSC_VER)
		#pragma warning(disable : 4251)
	#else
		#pragma GCC diagnostic ignored "-Wattributes"
	#endif

	#ifdef CORE_API_STATIC // Static linked library (.lib)
		#define CORE_API_EXPORT
	#elif defined(CORE_API_EXPORTING) // Dynamic linked library (.dll)
		#define CORE_API_EXPORT __declspec(dllexport)
	#else
		#define CORE_API_EXPORT __declspec(dllimport)
	#endif

	#define CORE_API_HIDDEN

#elif defined(__GNUC__) || defined(__clang__)
// Not Windows System

	#ifdef CORE_API_STATIC // Static library (.o)
		#define CORE_API_EXPORT
		#define CORE_API_HIDDEN
	#elif defined(CORE_API_EXPORTING) // Shared library (.so)
		#define CORE_API_EXPORT __attribute__ ((visibility("default")))
		#define CORE_API_HIDDEN __attribute__ ((visibility("hidden")))
	#else
		// If you use -fvisibility=hidden in GCC, exception handling and RTTI
		// would break if visibility wasn't set during export _and_ import
		// because GCC would immediately forget all type infos encountered.
		// See http://gcc.gnu.org/wiki/Visibility
		#define CORE_API_EXPORT __attribute__ ((visibility ("default")))
		#define CORE_API_HIDDEN
	#endif
#else
	#error Unknown compiler, please implement shared library macros
#endif  // Non-Windows

#endif // HEALTHGPS_CORE_VISIBILITY_H