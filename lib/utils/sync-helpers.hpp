#pragma once
#include "export-symbol-helper.hpp"

#include <mutex>

namespace advss {

// Helper used in macro segment edit widgets
#define GUARD_LOADING_AND_LOCK()       \
	if (_loading || !_entryData) { \
		return;                \
	}                              \
	auto lock = LockContext();

[[nodiscard]] EXPORT std::mutex *GetMutex();
[[nodiscard]] EXPORT std::lock_guard<std::mutex> LockContext();
[[nodiscard]] EXPORT std::unique_lock<std::mutex> *GetLoopLock();

} // namespace advss
