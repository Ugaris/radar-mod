#ifdef _WIN32
// On Windows, always use __declspec for DLL exports (works with all compilers)
#define DLL_EXPORT __declspec(dllexport)
// dllimport works for both functions and data through lib/moac.a / moac.lib
#define DLL_IMPORT __declspec(dllimport)
#else
// On Unix-like systems, use visibility attributes for GCC/Clang
#if __GNUC__ >= 4 || defined(__clang__)
#define DLL_EXPORT __attribute__((visibility("default")))
// 'extern' keeps data declarations from becoming tentative definitions:
// without it each mod gets its own hidden BSS copy of every client variable
// (deadly under -fvisibility=hidden) instead of importing the client's.
#define DLL_IMPORT extern
#else
#define DLL_EXPORT
#define DLL_IMPORT
#endif
#endif
