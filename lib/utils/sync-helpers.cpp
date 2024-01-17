#include "sync-helpers.hpp"

namespace advss {

std::mutex *GetSwitcherMutex();
std::unique_lock<std::mutex> *GetSwitcherLoopLock();

std::mutex *GetMutex()
{
	return GetSwitcherMutex();
}

std::lock_guard<std::mutex> LockContext()
{
	return std::lock_guard<std::mutex>(*GetSwitcherMutex());
}

std::unique_lock<std::mutex> *GetLoopLock()
{
	return GetSwitcherLoopLock();
}

} // namespace advss
