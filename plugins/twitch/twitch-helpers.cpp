#include "twitch-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "token.hpp"

#include <log-helper.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace {

static const int cacheTimeoutSeconds = 10;
static std::atomic_bool apiIsThrottling = {false};

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
	bool operator==(const Args &other) const
	{
		bool ret = true;
		ret = ret && _uri == other._uri;
		ret = ret && _path == other._path;
		ret = ret && _params == other._params;
		ret = ret && _data == other._data;
		ret = ret && _headers == other._headers;
		return ret;
	}

	const std::string &uri() const { return _uri; }
	const std::string &path() const { return _path; }
	const std::string &data() const { return _data; }
	const httplib::Params &params() const { return _params; }
	const httplib::Headers &headers() const { return _headers; }

private:
	std::string _uri;
	std::string _path;
	std::string _data;
	httplib::Params _params;
	httplib::Headers _headers;
};

}; // namespace

template<> struct std::hash<Args> {
	inline std::size_t operator()(const Args &args) const
	{
		static constexpr auto hash_combine = [](std::size_t &seed,
							std::size_t hashValue) {
			seed ^= hashValue + 0x9e3779b9 + (seed << 6) +
				(seed >> 2);
		};

		std::size_t seed = 0;
		hash_combine(seed, std::hash<std::string>()(args.uri()));
		hash_combine(seed, std::hash<std::string>()(args.path()));
		hash_combine(seed, std::hash<std::string>()(args.data()));

		for (const auto &[key, value] : args.params()) {
			hash_combine(seed, std::hash<std::string>()(key));
			hash_combine(seed, std::hash<std::string>()(value));
		}
		for (const auto &[key, value] : args.headers()) {
			hash_combine(seed, std::hash<std::string>()(key));
			hash_combine(seed, std::hash<std::string>()(value));
		}

		return seed;
	}
};

namespace advss {

static constexpr std::string_view clientID = "ds5tt4ogliifsqc04mz3d3etnck3e5";

const char *GetClientID()
{
	return clientID.data();
}

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

static bool cacheIsValid(const std::unordered_map<Args, CacheEntry> &cache,
			 const Args &args)
{
	auto it = cache.find(args);
	return it != cache.end() && !cacheIsTooOld(it->second);
}

static void cleanupCache(std::unordered_map<Args, CacheEntry> &cache,
			 std::mutex &mtx)
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

	static std::unordered_map<Args, CacheEntry> cache;
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
				     const std::string &data)
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
	auto response =
		cli.Post(pathWithParams, headers, data, "application/json");

	return processResult(response, __func__);
}

RequestResult SendPostRequest(const TwitchToken &token, const std::string &uri,
			      const std::string &path,
			      const httplib::Params &params,
			      const OBSData &data, bool useCache)
{
	auto body = getRequestBody(data);
	return SendPostRequest(token, uri, path, params, body, useCache);
}

RequestResult SendPostRequest(const TwitchToken &token, const std::string &uri,
			      const std::string &path,
			      const httplib::Params &params,
			      const std::string &data, bool useCache)
{
	if (apiIsThrottling) {
		return {};
	}

	static std::unordered_map<Args, CacheEntry> cache;
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
	Args args(uri, path, data, params, headers);

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
				    const std::string &data)
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
	auto response =
		cli.Put(pathWithParams, headers, data, "application/json");

	return processResult(response, __func__);
}

RequestResult SendPutRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params, const OBSData &data,
			     bool useCache)
{
	auto body = getRequestBody(data);
	return SendPutRequest(token, uri, path, params, body, useCache);
}

RequestResult SendPutRequest(const TwitchToken &token, const std::string &uri,
			     const std::string &path,
			     const httplib::Params &params,
			     const std::string &data, bool useCache)
{
	if (apiIsThrottling) {
		return {};
	}

	static std::unordered_map<Args, CacheEntry> cache;
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
	Args args(uri, path, data, params, headers);

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
				      const std::string &data)
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
	auto response =
		cli.Patch(pathWithParams, headers, data, "application/json");

	return processResult(response, __func__);
}

RequestResult SendPatchRequest(const TwitchToken &token, const std::string &uri,
			       const std::string &path,
			       const httplib::Params &params,
			       const OBSData &data, bool useCache)
{
	auto body = getRequestBody(data);
	return SendPatchRequest(token, uri, path, params, body, useCache);
}

RequestResult SendPatchRequest(const TwitchToken &token, const std::string &uri,
			       const std::string &path,
			       const httplib::Params &params,
			       const std::string &data, bool useCache)
{
	if (apiIsThrottling) {
		return {};
	}

	static std::unordered_map<Args, CacheEntry> cache;
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
	Args args(uri, path, data, params, headers);

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
