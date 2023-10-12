#include "twitch-helpers.hpp"
#include "token.hpp"

namespace advss {

static constexpr std::string_view clientID = "ds5tt4ogliifsqc04mz3d3etnck3e5";
static const int cacheTimeoutSeconds = 10;

class Args {
public:
	Args(const std::string &uri, const std::string &path,
	     const std::string &data, httplib::Params params,
	     httplib::Headers headers)
		: _uri(uri),
		  _path(path),
		  _data(data),
		  _params(params),
		  _headers(headers)
	{
	}
	bool operator<(const Args &other) const
	{
		bool ret = true;
		ret = ret && _uri < other._uri;
		ret = ret && _path < other._path;
		ret = ret && _params < other._params;
		ret = ret && _data < other._data;
		ret = ret && _headers < other._headers;
		return ret;
	}

private:
	std::string _uri;
	std::string _path;
	std::string _data;
	httplib::Params _params;
	httplib::Headers _headers;
};

struct CacheEntry {
	RequestResult result;
	std::chrono::system_clock::time_point cacheTime =
		std::chrono::system_clock::now();
};

static bool chacheIsTooOld(const CacheEntry &cache)
{
	auto currentTime = std::chrono::system_clock::now();
	auto diff = currentTime - cache.cacheTime;
	return diff >= std::chrono::seconds(cacheTimeoutSeconds);
}

static bool cacheIsValid(const std::map<Args, CacheEntry> &cache,
			 const Args &args)
{
	auto it = cache.find(args);
	return it != cache.end() && !chacheIsTooOld(it->second);
}

static httplib::Headers getTokenRequestHeaders(const std::string &token)
{
	return {
		{"Authorization", "Bearer " + token},
		{"Client-Id", clientID.data()},
	};
}

RequestResult SendGetRequest(const std::string &uri, const std::string &path,
			     const TwitchToken &token,
			     const httplib::Params &params)
{
	httplib::Client cli(uri);
	auto tokenStr = token.GetToken();
	if (!tokenStr) {
		return {};
	}
	auto headers = getTokenRequestHeaders(*tokenStr);
	auto response = cli.Get(path, params, headers);
	if (!response) {
		auto err = response.error();
		blog(LOG_INFO, "%s failed - %s", __func__,
		     httplib::to_string(err).c_str());
		return {};
	}
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

RequestResult SendGetRequest(const std::string &uri, const std::string &path,
			     const TwitchToken &token,
			     const httplib::Params &params, bool useCache)
{
	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	auto tokenStr = token.GetToken();
	if (!tokenStr) {
		return {};
	}
	auto headers = getTokenRequestHeaders(*tokenStr);
	Args args(uri, path, "", params, headers);
	if (useCache && cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = SendGetRequest(uri, path, token, params);
	cache[args] = {result};
	return result;
}

RequestResult SendPostRequest(const std::string &uri, const std::string &path,
			      const TwitchToken &token, const OBSData &data)
{
	httplib::Client cli(uri);
	auto tokenStr = token.GetToken();
	if (!tokenStr) {
		return {};
	}
	auto headers = getTokenRequestHeaders(*tokenStr);
	auto json = obs_data_get_json(data);
	std::string body = json ? json : "";
	auto response = cli.Post(path, headers, body, "application/json");
	if (!response) {
		auto err = response.error();
		blog(LOG_INFO, "%s failed - %s", __func__,
		     httplib::to_string(err).c_str());
		return {};
	}
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
			      const TwitchToken &token, const OBSData &data,
			      bool useCache)
{
	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	auto tokenStr = token.GetToken();
	if (!tokenStr) {
		return {};
	}
	auto headers = getTokenRequestHeaders(*tokenStr);
	auto jsonCstr = obs_data_get_json(data);
	Args args(uri, path, jsonCstr ? jsonCstr : "", {}, headers);
	if (useCache && cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = SendPostRequest(uri, path, token, data);
	cache[args] = {result};
	return result;
}

RequestResult SendPatchRequest(const std::string &uri, const std::string &path,
			       const TwitchToken &token, const OBSData &data)
{
	httplib::Client cli(uri);
	auto tokenStr = token.GetToken();
	if (!tokenStr) {
		return {};
	}
	auto headers = getTokenRequestHeaders(*tokenStr);
	auto json = obs_data_get_json(data);
	std::string body = json ? json : "";
	auto response = cli.Patch(path, headers, body, "application/json");
	if (!response) {
		auto err = response.error();
		blog(LOG_INFO, "%s failed - %s", __func__,
		     httplib::to_string(err).c_str());
		return {};
	}
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
			       const TwitchToken &token, const OBSData &data,
			       bool useCache)
{
	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	auto tokenStr = token.GetToken();
	if (!tokenStr) {
		return {};
	}
	auto headers = getTokenRequestHeaders(*tokenStr);
	auto jsonCstr = obs_data_get_json(data);
	Args args(uri, path, jsonCstr ? jsonCstr : "", {}, headers);
	if (useCache && cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = SendPatchRequest(uri, path, token, data);
	cache[args] = {result};
	return result;
}

const char *GetClientID()
{
	return clientID.data();
}

} // namespace advss
