#pragma once

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define GEE_HELPER_DLL_IMPORT __declspec(dllimport)
#define GEE_HELPER_DLL_EXPORT __declspec(dllexport)
#define GEE_HELPER_DLL_LOCAL
#else
#if __GNUC__ >= 4
#define GEE_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#define GEE_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#define GEE_HELPER_DLL_LOCAL __attribute__((visibility("hidden")))
#else
#define GEE_HELPER_DLL_IMPORT
#define GEE_HELPER_DLL_EXPORT
#define GEE_HELPER_DLL_LOCAL
#endif
#endif

// Now we use the generic helper definitions above to define GEE_API and GEE_LOCAL.
// GEE_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for
// static build) GEE_LOCAL is used for non-api symbols.

#ifdef GEE_DLL          // defined if GIGAEDGE_ENHANCE is compiled as a DLL
#ifdef GEE_DLL_EXPORTS  // defined if we are building the GIGAEDGE_ENHANCE DLL (instead of using it)
#define GEE_API GEE_HELPER_DLL_EXPORT
#else
#define GEE_API GEE_HELPER_DLL_IMPORT
#endif  // GEE_DLL_EXPORTS
#define GEE_LOCAL GEE_HELPER_DLL_LOCAL
#else  // GEE_DLL is not defined: this means GIGAEDGE_ENHANCE is a static lib.
#define GEE_API
#define GEE_LOCAL
#endif  // GEE_DLL
