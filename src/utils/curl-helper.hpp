#pragma once
#include <curl/curl.h>
#include <QLibrary>

namespace advss {

typedef CURL *(*initFunction)(void);
typedef void (*cleanupFunction)(CURL *);
typedef CURLcode (*setOptFunction)(CURL *, CURLoption, ...);
typedef struct curl_slist *(*slistAppendFunction)(struct curl_slist *list,
						  const char *string);
typedef CURLcode (*performFunction)(CURL *);

class Curlhelper {
public:
	Curlhelper();
	~Curlhelper();

	template<typename... Args> CURLcode SetOpt(CURLoption, Args...);
	struct curl_slist *SlistAppend(struct curl_slist *list,
				       const char *string);
	CURLcode Perform();
	bool Initialized() { return _initialized; }

private:
	bool LoadLib();
	bool Resolve();

	initFunction _init = nullptr;
	setOptFunction _setopt = nullptr;
	slistAppendFunction _slistAppend = nullptr;
	performFunction _perform = nullptr;
	cleanupFunction _cleanup = nullptr;
	CURL *_curl = nullptr;
	QLibrary *_lib;
	bool _initialized = false;
};

template<typename... Args>
inline CURLcode Curlhelper::SetOpt(CURLoption option, Args... args)
{
	if (!_initialized) {
		return CURLE_FAILED_INIT;
	}
	return _setopt(_curl, option, args...);
}

} // namespace advss
