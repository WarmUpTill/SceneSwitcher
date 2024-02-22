#pragma once

#ifdef UNIT_TEST

#define EXPORT
#define ADVSS_EXPORT

#else

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

// Helpers helper to enable exporting and importing custom Qt widget symbols
#ifdef ADVSS_EXPORT_SYMBOLS
#define ADVSS_EXPORT Q_DECL_EXPORT
#else
#define ADVSS_EXPORT Q_DECL_IMPORT
#endif

#endif // UNIT_TEST
