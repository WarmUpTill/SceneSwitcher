#pragma once
#include <mutex>

namespace advss {

[[nodiscard]] std::lock_guard<std::mutex> LockContext();

} // namespace advss
