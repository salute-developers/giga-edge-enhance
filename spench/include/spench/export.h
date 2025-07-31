#pragma once

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define SPENCH_HELPER_DLL_IMPORT __declspec(dllimport)
#define SPENCH_HELPER_DLL_EXPORT __declspec(dllexport)
#define SPENCH_HELPER_DLL_LOCAL
#else
#if __GNUC__ >= 4
#define SPENCH_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#define SPENCH_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#define SPENCH_HELPER_DLL_LOCAL __attribute__((visibility("hidden")))
#else
#define SPENCH_HELPER_DLL_IMPORT
#define SPENCH_HELPER_DLL_EXPORT
#define SPENCH_HELPER_DLL_LOCAL
#endif
#endif

// Now we use the generic helper definitions above to define SPENCH_API and SPENCH_LOCAL.
// SPENCH_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for static
// build) SPENCH_LOCAL is used for non-api symbols.

#ifdef SPENCH_DLL          // defined if SPENCH is compiled as a DLL
#ifdef SPENCH_DLL_EXPORTS  // defined if we are building the SPENCH DLL (instead of using it)
#define SPENCH_API SPENCH_HELPER_DLL_EXPORT
#else
#define SPENCH_API SPENCH_HELPER_DLL_IMPORT
#endif  // SPENCH_DLL_EXPORTS
#define SPENCH_LOCAL SPENCH_HELPER_DLL_LOCAL
#else  // SPENCH_DLL is not defined: this means SPENCH is a static lib.
#define SPENCH_API
#define SPENCH_LOCAL
#endif  // SPENCH_DLL