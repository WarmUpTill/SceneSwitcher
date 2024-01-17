#pragma once
#include "export-symbol-helper.hpp"

#include <curl/curl.h>
#include <QLibrary>
#include <atomic>

namespace advss {

class CurlHelper {
public:
	EXPORT static bool Initialized();
	template<typename... Args> static CURLcode SetOpt(CURLoption, Args...);
	EXPORT static struct curl_slist *SlistAppend(struct curl_slist *list,
						     const char *string);
	EXPORT static CURLcode Perform();
	EXPORT static char *GetError(CURLcode code);

private:
	CurlHelper();
	CurlHelper(const CurlHelper &) = delete;
	CurlHelper &operator=(const CurlHelper &) = delete;
	~CurlHelper();

	typedef CURL *(*initFunction)(void);
	typedef CURLcode (*setOptFunction)(CURL *, CURLoption, ...);
	typedef struct curl_slist *(*slistAppendFunction)(
		struct curl_slist *list, const char *string);
	typedef CURLcode (*performFunction)(CURL *);
	typedef void (*cleanupFunction)(CURL *);
	typedef char *(*errorFunction)(CURLcode);

	EXPORT static CurlHelper &GetInstance();

	bool LoadLib();
	bool Resolve();

	initFunction _init = nullptr;
	setOptFunction _setopt = nullptr;
	slistAppendFunction _slistAppend = nullptr;
	performFunction _perform = nullptr;
	cleanupFunction _cleanup = nullptr;
	errorFunction _error = nullptr;
	CURL *_curl = nullptr;
	QLibrary *_lib;
	std::atomic_bool _initialized = {false};
};

template<typename... Args>
inline CURLcode CurlHelper::SetOpt(CURLoption option, Args... args)
{
	auto &curl = GetInstance();
	if (!curl._initialized) {
		return CURLE_FAILED_INIT;
	}
	return curl._setopt(curl._curl, option, args...);
}

} // namespace advss
