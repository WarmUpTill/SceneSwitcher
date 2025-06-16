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

PerInstanceMutex::PerInstanceMutex() {}

PerInstanceMutex::~PerInstanceMutex(){};

PerInstanceMutex::PerInstanceMutex(const PerInstanceMutex &) {}

PerInstanceMutex &PerInstanceMutex::operator=(const PerInstanceMutex &)
{
	return *this;
}

PerInstanceMutex::operator std::mutex &()
{
	return _mtx;
}

PerInstanceMutex::operator const std::mutex &() const
{
	return _mtx;
}

std::lock_guard<std::mutex> Lockable::Lock()
{
	return std::lock_guard<std::mutex>(_mtx);
}

void Lockable::WithLock(const std::function<void()> &func)
{
	const auto lock = Lock();
	func();
}

} // namespace advss
