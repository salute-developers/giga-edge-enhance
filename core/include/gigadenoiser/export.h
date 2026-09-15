#pragma once

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define GDN_HELPER_DLL_IMPORT __declspec(dllimport)
#define GDN_HELPER_DLL_EXPORT __declspec(dllexport)
#define GDN_HELPER_DLL_LOCAL
#else
#if __GNUC__ >= 4
#define GDN_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#define GDN_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#define GDN_HELPER_DLL_LOCAL __attribute__((visibility("hidden")))
#else
#define GDN_HELPER_DLL_IMPORT
#define GDN_HELPER_DLL_EXPORT
#define GDN_HELPER_DLL_LOCAL
#endif
#endif

// Now we use the generic helper definitions above to define GDN_API and GDN_LOCAL.
// GDN_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for
// static build) GDN_LOCAL is used for non-api symbols.

#ifdef GDN_DLL          // defined if GIGA_DENOISER is compiled as a DLL
#ifdef GDN_DLL_EXPORTS  // defined if we are building the GIGA_DENOISER DLL (instead of using it)
#define GDN_API GDN_HELPER_DLL_EXPORT
#else
#define GDN_API GDN_HELPER_DLL_IMPORT
#endif  // GDN_DLL_EXPORTS
#define GDN_LOCAL GDN_HELPER_DLL_LOCAL
#else  // GDN_DLL is not defined: this means GIGA_DENOISER is a static lib.
#define GDN_API
#define GDN_LOCAL
#endif  // GDN_DLL
