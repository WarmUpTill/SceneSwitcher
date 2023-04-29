#include "sync-helper.hpp"

namespace advss {

std::mutex *GetSwitcherMutex();
std::lock_guard<std::mutex> LockContext()
{
	return std::lock_guard<std::mutex>(*GetSwitcherMutex());
}

} // namespace advss
