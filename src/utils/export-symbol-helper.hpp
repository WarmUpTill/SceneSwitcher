#pragma once

// Helpers helper to enable exporting and importing custom Qt widget symbols
#ifdef ADVSS_EXPORT_SYMBOLS
#define ADVSS_EXPORT Q_DECL_EXPORT
#else
#define ADVSS_EXPORT Q_DECL_IMPORT
#endif