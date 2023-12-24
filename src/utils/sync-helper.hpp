#pragma once
#include <mutex>

namespace advss {

[[nodiscard]] std::mutex *GetMutex();
[[nodiscard]] std::lock_guard<std::mutex> LockContext();
[[nodiscard]] std::unique_lock<std::mutex> *GetLoopLock();

} // namespace advss
