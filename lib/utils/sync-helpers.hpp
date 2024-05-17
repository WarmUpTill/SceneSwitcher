#pragma once
#include "export-symbol-helper.hpp"

#include <mutex>

namespace advss {

[[nodiscard]] EXPORT std::mutex *GetMutex();
[[nodiscard]] EXPORT std::lock_guard<std::mutex> LockContext();
[[nodiscard]] EXPORT std::unique_lock<std::mutex> *GetLoopLock();

} // namespace advss
