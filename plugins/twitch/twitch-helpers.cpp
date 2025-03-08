#include "twitch-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "token.hpp"

#include <log-helper.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace advss {

static constexpr std::string_view clientID = "ds5tt4ogliifsqc04mz3d3etnck3e5";
static const int cacheTimeoutSeconds = 10;
static std::atomic_bool apiIsThrottling = {false};

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

static void cleanupCache(std::map<Args, CacheEntry> &cache, std::mutex &mtx)
{
	std::lock_guard<std::mutex> lock(mtx);
	cache.clear();
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

static void handleThrottling(const httplib::Result &response)
{
	const auto &headers = response.value().headers;
	const auto rateLimitResetTimeStr = headers.find("Ratelimit-Reset");
	if (rateLimitResetTimeStr == headers.end()) {
		return;
	}

	std::time_t rateLimitResetTime =
		std::stoll(rateLimitResetTimeStr->second);
	auto resetTimePoint =
		std::chrono::system_clock::from_time_t(rateLimitResetTime);
	auto sleepDuration = resetTimePoint - std::chrono::system_clock::now();
	if (sleepDuration.count() < 0) {
		return;
	}

	blog(LOG_WARNING, "Twitch API access is throttled for %lld seconds!",
	     static_cast<long long int>(
		     std::chrono::duration_cast<std::chrono::seconds>(
			     sleepDuration)
			     .count()));
	apiIsThrottling = true;

	auto resetThread = std::thread([sleepDuration]() {
		std::this_thread::sleep_for(sleepDuration);
		apiIsThrottling = false;
	});
	resetThread.detach();
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

	if (response->status == 429) {
		handleThrottling(response);
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

static RequestResult sendGetRequest(const TwitchToken &token,
				    const std::string &uri,
				    const std::string &path,
				    const httplib::Params &params)
{
	if (apiIsThrottling) {
		return {};
	}

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
	if (apiIsThrottling) {
		return {};
	}

	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	[[maybe_unused]] static bool _ = []() {
		AddPluginCleanupStep([]() { cleanupCache(cache, mtx); });
		return true;
	}();

	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto headers = getTokenRequestHeaders(*tokenStr);
	Args args(uri, path, "", params, headers);

	if (!useCache) {
		return sendGetRequest(token, uri, path, params);
	}

	std::lock_guard<std::mutex> lock(mtx);
	if (cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = sendGetRequest(token, uri, path, params);
	cache[args] = {result};

	return result;
}

static RequestResult sendPostRequest(const TwitchToken &token,
				     const std::string &uri,
				     const std::string &path,
				     const httplib::Params &params,
				     const OBSData &data)
{
	if (apiIsThrottling) {
		return {};
	}

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
	if (apiIsThrottling) {
		return {};
	}

	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	[[maybe_unused]] static bool _ = []() {
		AddPluginCleanupStep([]() { cleanupCache(cache, mtx); });
		return true;
	}();

	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto body = getRequestBody(data);
	Args args(uri, path, body, params, headers);

	if (!useCache) {
		return sendPostRequest(token, uri, path, params, data);
	}

	std::lock_guard<std::mutex> lock(mtx);
	if (cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = sendPostRequest(token, uri, path, params, data);
	cache[args] = {result};

	return result;
}

static RequestResult sendPutRequest(const TwitchToken &token,
				    const std::string &uri,
				    const std::string &path,
				    const httplib::Params &params,
				    const OBSData &data)
{
	if (apiIsThrottling) {
		return {};
	}

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
	if (apiIsThrottling) {
		return {};
	}

	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	[[maybe_unused]] static bool _ = []() {
		AddPluginCleanupStep([]() { cleanupCache(cache, mtx); });
		return true;
	}();

	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto body = getRequestBody(data);
	Args args(uri, path, body, params, headers);

	if (!useCache) {
		return sendPutRequest(token, uri, path, params, data);
	}

	std::lock_guard<std::mutex> lock(mtx);
	if (cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = sendPutRequest(token, uri, path, params, data);
	cache[args] = {result};

	return result;
}

static RequestResult sendPatchRequest(const TwitchToken &token,
				      const std::string &uri,
				      const std::string &path,
				      const httplib::Params &params,
				      const OBSData &data)
{
	if (apiIsThrottling) {
		return {};
	}

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
	if (apiIsThrottling) {
		return {};
	}

	static std::map<Args, CacheEntry> cache;
	static std::mutex mtx;
	[[maybe_unused]] static bool _ = []() {
		AddPluginCleanupStep([]() { cleanupCache(cache, mtx); });
		return true;
	}();

	auto tokenStr = token.GetToken();

	if (!tokenStr) {
		return {};
	}

	auto headers = getTokenRequestHeaders(*tokenStr);
	auto body = getRequestBody(data);
	Args args(uri, path, body, params, headers);

	if (!useCache) {
		return sendPatchRequest(token, uri, path, params, data);
	}

	std::lock_guard<std::mutex> lock(mtx);
	if (cacheIsValid(cache, args)) {
		auto it = cache.find(args);
		return it->second.result;
	}

	auto result = sendPatchRequest(token, uri, path, params, data);
	cache[args] = {result};

	return result;
}

static RequestResult sendDeleteRequest(const TwitchToken &token,
				       const std::string &uri,
				       const std::string &path,
				       const httplib::Params &params)
{
	if (apiIsThrottling) {
		return {};
	}

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

void SetJsonTempVars(const std::string &jsonStr,
		     std::function<void(const char *, const char *)> setVarFunc)
{
	try {
		auto json = nlohmann::json::parse(jsonStr);
		for (auto it = json.begin(); it != json.end(); ++it) {
			if (it->is_string()) {
				setVarFunc(it.key().c_str(),
					   it->get<std::string>().c_str());
				continue;
			}
			setVarFunc(it.key().c_str(), it->dump().c_str());
		}
	} catch (const nlohmann::json::exception &e) {
		vblog(LOG_INFO, "%s", jsonStr.c_str());
		vblog(LOG_INFO, "%s", e.what());
	}
}

void SetJsonTempVars(obs_data_t *data,
		     std::function<void(const char *, const char *)> setVarFunc)
{
	auto jsonStr = obs_data_get_json(data);
	if (!jsonStr) {
		return;
	}

	SetJsonTempVars(jsonStr, setVarFunc);
}

} // namespace advss
