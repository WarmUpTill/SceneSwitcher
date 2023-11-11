#include "curl-helper.hpp"

#include <QDir>
#include <QFileInfo>
#include <curl/curl.h>
#include <obs.hpp>

namespace advss {

#if defined(WIN32)
constexpr auto curl_library_name = "libcurl.dll";
#elif __APPLE__
constexpr auto curl_library_name = "libcurl.4.dylib";
#else
constexpr auto curl_library_name = "libcurl.so.4";
#endif

Curlhelper::Curlhelper()
{
	if (LoadLib()) {
		_curl = _init();
		_initialized = true;
	}
}

Curlhelper::~Curlhelper()
{
	if (_lib) {
		if (_cleanup) {
			_cleanup(_curl);
		}
		delete _lib;
		_lib = nullptr;
	}
}

curl_slist *Curlhelper::SlistAppend(curl_slist *list, const char *string)
{
	if (!_initialized) {
		return nullptr;
	}
	return _slistAppend(list, string);
}

CURLcode Curlhelper::Perform()
{
	if (!_initialized) {
		return CURLE_FAILED_INIT;
	}
	return _perform(_curl);
}

char *Curlhelper::GetError(CURLcode code)
{
	if (!_initialized) {
		return (char *)"CURL initialization failed";
	}

	return _error(code);
}

bool Curlhelper::LoadLib()
{
	_lib = new QLibrary(curl_library_name, nullptr);
	if (Resolve()) {
		blog(LOG_INFO, "[adv-ss] found curl library");
		return true;
	} else {
		delete _lib;
		_lib = nullptr;
		blog(LOG_WARNING,
		     "[adv-ss] couldn't find the curl library in PATH");
	}
	QStringList locations;
	locations << QDir::currentPath();
#if defined(__linux__) || defined(__APPLE__)
	locations << "/usr/lib";
	locations << "/usr/local/lib";
	locations << "/usr/lib/x86_64-linux-gnu";
	locations << "/usr/local/opt/curl/lib";
#endif
	for (QString path : locations) {
		blog(LOG_INFO, "[adv-ss] trying '%s'",
		     path.toUtf8().constData());
		QFileInfo libPath(
			QDir(path).absoluteFilePath(curl_library_name));

		if (libPath.exists() && libPath.isFile()) {
			QString libFilePath = libPath.absoluteFilePath();
			blog(LOG_INFO, "[adv-ss] found curl library at '%s'",
			     libFilePath.toUtf8().constData());

			_lib = new QLibrary(libFilePath, nullptr);
			if (Resolve()) {
				return true;
			} else {
				delete _lib;
				_lib = nullptr;
			}
		}
	}
	blog(LOG_WARNING, "[adv-ss] can't find the curl library");
	return false;
}

bool Curlhelper::Resolve()
{
	_init = (initFunction)_lib->resolve("curl_easy_init");
	_setopt = (setOptFunction)_lib->resolve("curl_easy_setopt");
	_slistAppend = (slistAppendFunction)_lib->resolve("curl_slist_append");
	_perform = (performFunction)_lib->resolve("curl_easy_perform");
	_cleanup = (cleanupFunction)_lib->resolve("curl_easy_cleanup");
	_error = (errorFunction)_lib->resolve("curl_easy_strerror");

	if (_init && _setopt && _slistAppend && _perform && _cleanup &&
	    _error) {
		blog(LOG_INFO, "[adv-ss] curl loaded successfully");
		return true;
	}

	blog(LOG_INFO, "[adv-ss] curl symbols not resolved");
	return false;
}

} // namespace advss
