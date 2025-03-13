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

const char *GetClientID();

// These functions can cache the RequestResult for 10s

RequestResult SendGetRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params = {},
			     bool useCache = false);
RequestResult SendPostRequest(const TwitchToken &token, const std::string &uri,
			      const std::string &path,
			      const httplib::Params &params = {},
			      const OBSData &data = nullptr,
			      bool useCache = false);
RequestResult SendPutRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params = {},
			     const OBSData &data = nullptr,
			     bool useCache = false);
RequestResult SendPatchRequest(const TwitchToken &token, const std::string &uri,
			       const std::string &path,
			       const httplib::Params &params = {},
			       const OBSData &data = nullptr,
			       bool useCache = false);

// Helper functions to set temp var values

void SetJsonTempVars(const std::string &jsonStr,
		     std::function<void(const char *, const char *)> setVarFunc);
void SetJsonTempVars(obs_data_t *data,
		     std::function<void(const char *, const char *)> setVarFunc);

} // namespace advss
