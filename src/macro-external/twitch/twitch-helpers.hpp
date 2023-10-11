#pragma once
#include <httplib.h>
#include <obs.hpp>
#include <string>
#include <chrono>

namespace advss {

class TwitchToken;

struct RequestResult {
	int status = 0;
	OBSData data = nullptr;
};

// These functions do *not* use or create RequestResult cache entries

RequestResult SendGetRequest(const std::string &uri, const std::string &path,
			     const TwitchToken &token,
			     const httplib::Params & = {});
RequestResult SendPostRequest(const std::string &uri, const std::string &path,
			      const TwitchToken &token, const OBSData &data);
RequestResult SendPatchRequest(const std::string &uri, const std::string &path,
			       const TwitchToken &token, const OBSData &data);

// These functions will cache the RequestResult for 10s
// Note that the cache will be reported as a "memory leak" on OBS shutdown

RequestResult SendGetRequest(const std::string &uri, const std::string &path,
			     const TwitchToken &token, const httplib::Params &,
			     bool useCache);
RequestResult SendPostRequest(const std::string &uri, const std::string &path,
			      const TwitchToken &token, const OBSData &data,
			      bool useCache);
RequestResult SendPatchRequest(const std::string &uri, const std::string &path,
			       const TwitchToken &token, const OBSData &data,
			       bool useCache);

const char *GetClientID();

} // namespace advss
