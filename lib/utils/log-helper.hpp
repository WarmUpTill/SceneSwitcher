#pragma once
#ifndef UNIT_TEST
#include <util/base.h>
#endif

namespace advss {

#ifdef UNIT_TEST
#define blog(level, msg, ...)
#define vblog(level, msg, ...)
#define ablog(level, msg, ...)
#else

// Print log with "[adv-ss] " prefix
#define blog(level, msg, ...) blog(level, "[adv-ss] " msg, ##__VA_ARGS__)
// Print log with "[adv-ss] " if log level is set to "verbose"
#define vblog(level, msg, ...)                           \
	do {                                             \
		if (VerboseLoggingEnabled()) {           \
			blog(level, msg, ##__VA_ARGS__); \
		}                                        \
	} while (0)
// Print log with "[adv-ss] " if log level is set to "action" or "verbose"
#define ablog(level, msg, ...)                           \
	do {                                             \
		if (ActionLoggingEnabled()) {            \
			blog(level, msg, ##__VA_ARGS__); \
		}                                        \
	} while (0)

#endif

// Returns true if log level is set to "verbose"
EXPORT bool VerboseLoggingEnabled();
// Returns true if log level is set to "action" or "verbose"
EXPORT bool ActionLoggingEnabled();

} // namespace advss
