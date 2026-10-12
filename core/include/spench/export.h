#pragma once

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define GES_HELPER_DLL_IMPORT __declspec(dllimport)
#define GES_HELPER_DLL_EXPORT __declspec(dllexport)
#define GES_HELPER_DLL_LOCAL
#else
#if __GNUC__ >= 4
#define GES_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#define GES_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#define GES_HELPER_DLL_LOCAL __attribute__((visibility("hidden")))
#else
#define GES_HELPER_DLL_IMPORT
#define GES_HELPER_DLL_EXPORT
#define GES_HELPER_DLL_LOCAL
#endif
#endif

// Now we use the generic helper definitions above to define GES_API and GES_LOCAL.
// GES_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for
// static build) GES_LOCAL is used for non-api symbols.

#ifdef GES_DLL          // defined if GIGAEDGE_SPENCH is compiled as a DLL
#ifdef GES_DLL_EXPORTS  // defined if we are building the GIGAEDGE_SPENCH DLL (instead of using it)
#define GES_API GES_HELPER_DLL_EXPORT
#else
#define GES_API GES_HELPER_DLL_IMPORT
#endif  // GES_DLL_EXPORTS
#define GES_LOCAL GES_HELPER_DLL_LOCAL
#else  // GES_DLL is not defined: this means GIGAEDGE_SPENCH is a static lib.
#define GES_API
#define GES_LOCAL
#endif  // GES_DLL
