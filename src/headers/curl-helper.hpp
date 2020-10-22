#pragma once
#include <curl/curl.h>
#include <QLibrary>

#if defined(WIN32)
constexpr auto curl_library_name = "libcurl.dll";
#elif __APPLE__
#define curl_library_name "libcurl.4.dylib"
#else
#define curl_library_name "libcurl.so.4"
#endif

typedef CURL *(*initFunction)(void);
typedef CURLcode (*setOptFunction)(CURL *, CURLoption, ...);
typedef CURLcode (*performFunction)(CURL *);
typedef void (*cleanupFunction)(CURL *);

extern initFunction f_curl_init;
extern setOptFunction f_curl_setopt;
extern performFunction f_curl_perform;
extern cleanupFunction f_curl_cleanup;

extern QLibrary *loaded_curl_lib;

bool resolveCurl();
bool loadCurl();
