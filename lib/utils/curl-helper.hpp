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
	EXPORT static void Reset();
	EXPORT static char *GetError(CURLcode code);
	template<typename... Args> static CURLcode GetInfo(CURLINFO, Args...);

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
	typedef void (*resetFunction)(CURL *);
	typedef char *(*errorFunction)(CURLcode);
	typedef CURLcode (*getInfoFunction)(CURL *, CURLINFO, ...);

	EXPORT static CurlHelper &GetInstance();

	bool LoadLib();
	bool Resolve();

	CURL *_curl = nullptr;
	QLibrary *_lib;
	std::atomic_bool _initialized = {false};

	initFunction _init = nullptr;
	setOptFunction _setOpt = nullptr;
	slistAppendFunction _slistAppend = nullptr;
	performFunction _perform = nullptr;
	cleanupFunction _cleanup = nullptr;
	resetFunction _reset = nullptr;
	errorFunction _error = nullptr;
	getInfoFunction _getInfo = nullptr;
};

template<typename... Args>
inline CURLcode CurlHelper::SetOpt(CURLoption option, Args... args)
{
	auto &curl = GetInstance();
	if (!curl._initialized) {
		return CURLE_FAILED_INIT;
	}

	return curl._setOpt(curl._curl, option, args...);
}

template<typename... Args>
inline CURLcode CurlHelper::GetInfo(CURLINFO info, Args... args)
{
	auto &curl = GetInstance();
	if (!curl._initialized) {
		return CURLE_FAILED_INIT;
	}

	return curl._getInfo(curl._curl, info, args...);
}

} // namespace advss
