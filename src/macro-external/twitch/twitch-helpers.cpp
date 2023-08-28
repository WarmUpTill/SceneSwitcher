#include "twitch-helpers.hpp"
#include "token.hpp"

namespace advss {

static constexpr std::string_view clientID = "ds5tt4ogliifsqc04mz3d3etnck3e5";

static httplib::Headers getTokenRequestHeaders(const TwitchToken &token)
{
	return {
		{"Authorization", "Bearer " + token.GetToken()},
		{"Client-Id", clientID.data()},
	};
}

RequestResult SendGetRequest(const std::string &uri, const std::string &path,
			     const TwitchToken &token,
			     const httplib::Params &params)
{
	httplib::Client cli(uri);
	auto headers = getTokenRequestHeaders(token);
	auto response = cli.Get(path, params, headers);
	RequestResult result;
	result.status = response->status;
	if (response->body.empty()) {
		return result;
	}
	OBSDataAutoRelease replyData =
		obs_data_create_from_json(response->body.c_str());
	result.data = replyData;
	return result;
}

RequestResult SendPostRequest(const std::string &uri, const std::string &path,
			      const TwitchToken &token, const OBSData &data)
{
	httplib::Client cli(uri);
	auto headers = getTokenRequestHeaders(token);
	auto json = obs_data_get_json(data);
	std::string body = json ? json : "";
	auto response = cli.Post(path, headers, body, "application/json");
	RequestResult result;
	result.status = response->status;
	if (response->body.empty()) {
		return result;
	}
	OBSDataAutoRelease replyData =
		obs_data_create_from_json(response->body.c_str());
	result.data = replyData;
	return result;
}

RequestResult SendPatchRequest(const std::string &uri, const std::string &path,
			       const TwitchToken &token, const OBSData &data)
{
	httplib::Client cli(uri);
	auto headers = getTokenRequestHeaders(token);
	auto json = obs_data_get_json(data);
	std::string body = json ? json : "";
	auto response = cli.Patch(path, headers, body, "application/json");
	RequestResult result;
	result.status = response->status;
	if (response->body.empty()) {
		return result;
	}
	OBSDataAutoRelease replyData =
		obs_data_create_from_json(response->body.c_str());
	result.data = replyData;
	return result;
}

const char *GetClientID()
{
	return clientID.data();
}

} // namespace advss
