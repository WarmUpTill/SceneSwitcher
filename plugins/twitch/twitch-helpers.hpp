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

// These functions do *not* use or create RequestResult cache entries
RequestResult SendGetRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params = {});
RequestResult SendPostRequest(const TwitchToken &token, const std::string &uri,
			      const std::string &path,
			      const httplib::Params &params = {},
			      const OBSData &data = nullptr);
RequestResult SendPutRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params = {},
			     const OBSData &data = nullptr);
RequestResult SendPatchRequest(const TwitchToken &token, const std::string &uri,
			       const std::string &path,
			       const httplib::Params &params = {},
			       const OBSData &data = nullptr);
RequestResult SendDeleteRequest(const TwitchToken &token,
				const std::string &uri, const std::string &path,
				const httplib::Params &params = {});

// These functions will cache the RequestResult for 10s
// Note that the cache will be reported as a "memory leak" on OBS shutdown
RequestResult SendGetRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params, bool useCache);
RequestResult SendPostRequest(const TwitchToken &token, const std::string &uri,
			      const std::string &path,
			      const httplib::Params &params,
			      const OBSData &data, bool useCache);
RequestResult SendPutRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params, const OBSData &data,
			     bool useCache);
RequestResult SendPatchRequest(const TwitchToken &token, const std::string &uri,
			       const std::string &path,
			       const httplib::Params &params,
			       const OBSData &data, bool useCache);

// Helper functions to set temp var values
void SetJsonTempVars(const std::string &jsonStr,
		     std::function<void(const char *, const char *)> setVarFunc);
void SetJsonTempVars(obs_data_t *data,
		     std::function<void(const char *, const char *)> setVarFunc);

} // namespace advss
