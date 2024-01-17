#pragma once
#include "export-symbol-helper.hpp"

#include <mutex>

namespace advss {

EXPORT std::mutex *GetMutex();
EXPORT std::lock_guard<std::mutex> LockContext();
EXPORT std::unique_lock<std::mutex> *GetLoopLock();

} // namespace advss
