#pragma once
#include "export-symbol-helper.hpp"

#include <functional>
#include <memory>
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
};

} // namespace advss
