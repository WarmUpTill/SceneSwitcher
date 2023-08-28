#pragma once
#include <httplib.h>
#include <obs.hpp>
#include <string>

namespace advss {

class TwitchToken;

struct RequestResult {
	int status = 0;
	OBSData data = nullptr;
};

RequestResult SendGetRequest(const std::string &uri, const std::string &path,
			     const TwitchToken &token,
			     const httplib::Params & = {});
RequestResult SendPostRequest(const std::string &uri, const std::string &path,
			      const TwitchToken &token, const OBSData &data);
RequestResult SendPatchRequest(const std::string &uri, const std::string &path,
			       const TwitchToken &token, const OBSData &data);
const char *GetClientID();

} // namespace advss
