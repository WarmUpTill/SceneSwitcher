#pragma once
#include "export-symbol-helper.hpp"

#include <functional>
#include <mutex>

namespace advss {

// Helper used in macro segment edit widgets
#define GUARD_LOADING_AND_LOCK()       \
	if (_loading || !_entryData) { \
		return;                \
	}                              \
	auto lock = _entryData->Lock();

[[nodiscard]] EXPORT std::mutex *GetMutex();
[[nodiscard]] EXPORT std::lock_guard<std::mutex> LockContext();
[[nodiscard]] EXPORT std::unique_lock<std::mutex> *GetLoopLock();

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

// std::mutex wrapper with no-op copy constructor and assignment operator
class EXPORT PerInstanceMutex {
public:
	PerInstanceMutex();
	~PerInstanceMutex();
	PerInstanceMutex(const PerInstanceMutex &);
	PerInstanceMutex &operator=(const PerInstanceMutex &);

	operator std::mutex &();
	operator const std::mutex &() const;

private:
	std::mutex _mtx;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

class EXPORT Lockable {
public:
	Lockable() = default;
	virtual ~Lockable() = default;

	[[nodiscard]] std::lock_guard<std::mutex> Lock();
	void WithLock(const std::function<void()> &func);

private:
	PerInstanceMutex _mtx;

	friend class SuspendLock;
};

// RAII guard that temporarily releases a Lockable's per-segment lock.
// Use this inside PerformAction() / CheckCondition() to unblock the UI
// during long-running operations while still running on the original segment.
// The caller MUST be holding the lock (i.e. be inside WithLock) when
// constructing this object; the lock is re-acquired on destruction.
class EXPORT SuspendLock {
public:
	SuspendLock(Lockable &lockable);
	~SuspendLock();
	SuspendLock(const SuspendLock &) = delete;
	SuspendLock &operator=(const SuspendLock &) = delete;

private:
	std::mutex &_mtx;
};

} // namespace advss
