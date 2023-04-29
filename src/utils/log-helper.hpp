#pragma once
#include <util/base.h>

namespace advss {

bool VerboseLoggingEnabled();

#define blog(level, msg, ...) blog(level, "[adv-ss] " msg, ##__VA_ARGS__)
#define vblog(level, msg, ...)                           \
	do {                                             \
		if (VerboseLoggingEnabled()) {           \
			blog(level, msg, ##__VA_ARGS__); \
		}                                        \
	} while (0)

} // namespace advss
