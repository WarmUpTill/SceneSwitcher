#include <QDir>
#include <QFileInfo>
#include <curl/curl.h>
#include <obs.hpp>

#include "headers/curl-helper.hpp"

initFunction f_curl_init = nullptr;
setOptFunction f_curl_setopt = nullptr;
performFunction f_curl_perform = nullptr;
cleanupFunction f_curl_cleanup = nullptr;

QLibrary *loaded_curl_lib = nullptr;

bool resolveCurl()
{
	f_curl_init = (initFunction)loaded_curl_lib->resolve("curl_easy_init");
	f_curl_setopt = (setOptFunction)loaded_curl_lib->resolve("curl_easy_setopt");
	f_curl_perform =
		(performFunction)loaded_curl_lib->resolve("curl_easy_perform");
	f_curl_cleanup =
		(cleanupFunction)loaded_curl_lib->resolve("curl_easy_cleanup");

	if (f_curl_init && f_curl_setopt && f_curl_perform && f_curl_cleanup) {
		blog(LOG_INFO, "curl loaded successfully");
		return true;
	}

	blog(LOG_INFO, "curl symbols not resolved");
	return false;
}

bool loadCurl()
{
	QStringList locations;
	locations << QDir::currentPath();
#if defined(__linux__) || defined(__APPLE__)
	locations << "/usr/lib";
	locations << "/usr/local/lib";
	locations << "/usr/lib/x86_64-linux-gnu";
	locations << "/usr/local/opt/curl/lib";
#endif

	for (QString path : locations) {
		blog(LOG_INFO, "trying '%s'", path.toUtf8().constData());
		QFileInfo libPath(QDir(path).absoluteFilePath(CURL_LIBRARY_NAME));

		if (libPath.exists() && libPath.isFile()) {
			QString libFilePath = libPath.absoluteFilePath();
			blog(LOG_INFO, "found curl library at '%s'",
			     libFilePath.toUtf8().constData());

			loaded_curl_lib = new QLibrary(libFilePath, nullptr);
			if (resolveCurl())
				return true;
			else {
				delete loaded_curl_lib;
				loaded_curl_lib = nullptr;
			}
		}
	}

	blog(LOG_ERROR, "can't find the curl library");
	return false;
}
