#include "twitch-helpers.hpp"
#include "token.hpp"

#include <log-helper.hpp>

namespace advss {

static constexpr std::string_view clientID = "ds5tt4ogliifsqc04mz3d3etnck3e5";
static const int cacheTimeoutSeconds = 10;

const char *GetClientID()
{
	return clientID.data();
}

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

static bool cacheIsTooOld(const CacheEntry &cache)
{
	auto currentTime = std::chrono::system_clock::now();
	auto diff = currentTime - cache.cacheTime;
	return diff >= std::chrono::seconds(cacheTimeoutSeconds);
}

static bool cacheIsValid(const std::map<Args, CacheEntry> &cache,
			 const Args &args)
{
	auto it = cache.find(args);
	return it != cache.end() && !cacheIsTooOld(it->second);
}

static httplib::Headers getTokenRequestHeaders(const std::string &token)
{
	return {
		{"Authorization", "Bearer " + token},
		{"Client-Id", clientID.data()},
	};
}

static std::string getRequestBody(const OBSData &data)
{
	auto json = obs_data_get_json(data);
	return json ? json : "";
}

static RequestResult processResult(const httplib::Result &response,
				   const char *funcName)
{
	if (!response) {
		auto err = response.error();
		blog(LOG_WARNING, "Twitch request failed in %s with error: %s",
		     funcName, httplib::to_string(err).c_str());

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

RequestResult SendGetRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params)
{
	httplib::Client cli(uri);
	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto url = httplib::append_query_params(uri + path, params);
	vblog(LOG_INFO, "Twitch GET request to %s began", url.c_str());

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto response = cli.Get(path, params, headers);

	return processResult(response, __func__);
}

RequestResult SendGetRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
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

	auto result = SendGetRequest(token, uri, path, params);
	cache[args] = {result};

	return result;
}

RequestResult SendPostRequest(const TwitchToken &token, const std::string &uri,
			      const std::string &path,
			      const httplib::Params &params,
			      const OBSData &data)
{
	httplib::Client cli(uri);
	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto pathWithParams = httplib::append_query_params(path, params);
	auto url = uri + pathWithParams;
	vblog(LOG_INFO, "Twitch POST request to %s began", url.c_str());

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto body = getRequestBody(data);
	auto response =
		cli.Post(pathWithParams, headers, body, "application/json");

	return processResult(response, __func__);
}

RequestResult SendPostRequest(const TwitchToken &token, const std::string &uri,
			      const std::string &path,
			      const httplib::Params &params,
			      const OBSData &data, bool useCache)
{
	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto body = getRequestBody(data);
	Args args(uri, path, body, params, headers);

	if (useCache && cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = SendPostRequest(token, uri, path, params, data);
	cache[args] = {result};

	return result;
}

RequestResult SendPutRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params, const OBSData &data)
{
	httplib::Client cli(uri);
	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto pathWithParams = httplib::append_query_params(path, params);
	auto url = uri + pathWithParams;
	vblog(LOG_INFO, "Twitch PUT request to %s began", url.c_str());

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto body = getRequestBody(data);
	auto response =
		cli.Put(pathWithParams, headers, body, "application/json");

	return processResult(response, __func__);
}

RequestResult SendPutRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params, const OBSData &data,
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
	auto body = getRequestBody(data);
	Args args(uri, path, body, params, headers);

	if (useCache && cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = SendPutRequest(token, uri, path, params, data);
	cache[args] = {result};

	return result;
}

RequestResult SendPatchRequest(const TwitchToken &token, const std::string &uri,
			       const std::string &path,
			       const httplib::Params &params,
			       const OBSData &data)
{
	httplib::Client cli(uri);
	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto pathWithParams = httplib::append_query_params(path, params);
	auto url = uri + pathWithParams;
	vblog(LOG_INFO, "Twitch PATCH request to %s began", url.c_str());

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto body = getRequestBody(data);
	auto response =
		cli.Patch(pathWithParams, headers, body, "application/json");

	return processResult(response, __func__);
}

RequestResult SendPatchRequest(const TwitchToken &token, const std::string &uri,
			       const std::string &path,
			       const httplib::Params &params,
			       const OBSData &data, bool useCache)
{
	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto body = getRequestBody(data);
	Args args(uri, path, body, params, headers);

	if (useCache && cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = SendPatchRequest(token, uri, path, params, data);
	cache[args] = {result};

	return result;
}

RequestResult SendDeleteRequest(const TwitchToken &token,
				const std::string &uri, const std::string &path,
				const httplib::Params &params)
{
	httplib::Client cli(uri);
	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto pathWithParams = httplib::append_query_params(path, params);
	auto url = uri + pathWithParams;
	vblog(LOG_INFO, "Twitch DELETE request to %s began", url.c_str());

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto response =
		cli.Delete(pathWithParams, headers, "", "application/json");

	return processResult(response, __func__);
}

} // namespace advss
