#include "youtube-helpers.hpp"
#include "youtube-connection.hpp"

#include <log-helper.hpp>
#include <plugin-state-helpers.hpp>

#include <mutex>
#include <unordered_map>

namespace advss {

static const char *tokenHost = "https://oauth2.googleapis.com";
static const char *apiHost = "https://www.googleapis.com";
static const char *apiBasePath = "/youtube/v3";
static const char *uploadBasePath = "/upload/youtube/v3";
// Conditions are checked frequently, so cache responses to stay within the
// daily API quota
static const int cacheTimeoutSeconds = 30;
static const int quotaPauseMinutes = 15;

static bool isQuotaError(const std::string &errorReason)
{
	return errorReason == "quotaExceeded" ||
	       errorReason == "rateLimitExceeded" ||
	       errorReason == "userRateLimitExceeded";
}

static TokenResponse sendTokenRequest(const httplib::Params &params)
{
	httplib::Client cli(tokenHost);
	const auto response = cli.Post("/token", params);
	if (!response) {
		blog(LOG_WARNING, "YouTube token request failed: %s",
		     httplib::to_string(response.error()).c_str());
		TokenResponse result;
		result.error = "network_error";
		return result;
	}

	auto result = ParseTokenResponse(response->body);
	if (!result.error.empty()) {
		blog(LOG_WARNING, "YouTube token request failed: %s (%s)",
		     result.error.c_str(), result.errorDescription.c_str());
	}
	return result;
}

TokenResponse ExchangeAuthCode(const OAuthClientConfig &config,
			       const std::string &code,
			       const std::string &codeVerifier,
			       const std::string &redirectUri)
{
	return sendTokenRequest({
		{"grant_type", "authorization_code"},
		{"code", code},
		{"code_verifier", codeVerifier},
		{"redirect_uri", redirectUri},
		{"client_id", config.clientId},
		{"client_secret", config.clientSecret},
	});
}

TokenResponse RefreshAccessToken(const OAuthClientConfig &config,
				 const std::string &refreshToken)
{
	return sendTokenRequest({
		{"grant_type", "refresh_token"},
		{"refresh_token", refreshToken},
		{"client_id", config.clientId},
		{"client_secret", config.clientSecret},
	});
}

void RevokeToken(const std::string &token)
{
	httplib::Client cli(tokenHost);
	const auto response =
		cli.Post("/revoke", httplib::Params{{"token", token}});
	if (!response) {
		blog(LOG_WARNING, "failed to revoke YouTube token: %s",
		     httplib::to_string(response.error()).c_str());
		return;
	}
	if (response->status != 200) {
		blog(LOG_WARNING, "failed to revoke YouTube token: %d",
		     response->status);
	}
}

static std::string getErrorReason(obs_data_t *data)
{
	OBSDataAutoRelease error = obs_data_get_obj(data, "error");
	OBSDataArrayAutoRelease errors = obs_data_get_array(error, "errors");
	OBSDataAutoRelease firstError = obs_data_array_item(errors, 0);
	return obs_data_get_string(firstError, "reason");
}

struct RequestBody {
	const std::string &data;
	const char *contentType = "application/json";
};

static httplib::Result
sendRequest(YouTubeRequestMethod method, const std::string &basePath,
	    const std::string &path, const httplib::Params &params,
	    const RequestBody &body, const std::string &accessToken)
{
	httplib::Client cli(apiHost);
	const httplib::Headers headers{
		{"Authorization", "Bearer " + accessToken}};
	const auto fullPath =
		httplib::append_query_params(basePath + path, params);

	switch (method) {
	case YouTubeRequestMethod::GET:
		vblog(LOG_INFO, "YouTube GET request to %s", fullPath.c_str());
		return cli.Get(fullPath, headers);
	case YouTubeRequestMethod::POST:
		vblog(LOG_INFO, "YouTube POST request to %s", fullPath.c_str());
		return cli.Post(fullPath, headers, body.data, body.contentType);
	case YouTubeRequestMethod::PUT:
		vblog(LOG_INFO, "YouTube PUT request to %s", fullPath.c_str());
		return cli.Put(fullPath, headers, body.data, body.contentType);
	}
	return httplib::Result();
}

static YouTubeRequestResult
sendYouTubeRequest(YouTubeConnection &connection, YouTubeRequestMethod method,
		   const std::string &basePath, const std::string &path,
		   const httplib::Params &params, const RequestBody &body)
{
	if (connection.RequestsArePaused()) {
		vblog(LOG_INFO, "skipping YouTube request to %s due to quota",
		      path.c_str());
		return {};
	}

	auto accessToken = connection.GetAccessToken();
	if (!accessToken) {
		return {};
	}

	auto response =
		sendRequest(method, basePath, path, params, body, *accessToken);
	if (response && response->status == 401) {
		accessToken = connection.GetAccessToken(true);
		if (!accessToken) {
			return {};
		}
		response = sendRequest(method, basePath, path, params, body,
				       *accessToken);
	}

	if (!response) {
		blog(LOG_WARNING, "YouTube request to %s failed: %s",
		     path.c_str(),
		     httplib::to_string(response.error()).c_str());
		return {};
	}

	YouTubeRequestResult result;
	result.status = response->status;
	if (response->body.empty()) {
		return result;
	}

	OBSDataAutoRelease data =
		obs_data_create_from_json(response->body.c_str());
	result.data = data;

	if (response->status < 200 || response->status >= 300) {
		result.errorReason = getErrorReason(data);
		blog(LOG_WARNING, "YouTube request to %s failed: %d %s",
		     path.c_str(), response->status,
		     result.errorReason.c_str());
		vblog(LOG_INFO, "YouTube API error response: %s",
		      response->body.c_str());
	}

	if (isQuotaError(result.errorReason)) {
		blog(LOG_WARNING,
		     "pausing requests of YouTube connection \"%s\" for %d minutes",
		     connection.Name().c_str(), quotaPauseMinutes);
		connection.PauseRequests(
			std::chrono::minutes(quotaPauseMinutes));
	}
	return result;
}

struct CacheEntry {
	YouTubeRequestResult result;
	std::chrono::steady_clock::time_point time;
};

static std::string getCacheKey(const YouTubeConnection &connection,
			       const std::string &path,
			       const httplib::Params &params)
{
	return connection.GetChannelID() +
	       httplib::append_query_params(path, params);
}

YouTubeRequestResult SendYouTubeRequest(YouTubeConnection &connection,
					YouTubeRequestMethod method,
					const std::string &path,
					const httplib::Params &params,
					const std::string &body, bool useCache)
{
	if (!useCache || method != YouTubeRequestMethod::GET) {
		return sendYouTubeRequest(connection, method, apiBasePath, path,
					  params, {body});
	}

	static std::mutex mutex;
	static std::unordered_map<std::string, CacheEntry> cache;
	[[maybe_unused]] static bool cleanupRegistered = []() {
		AddPluginCleanupStep([]() {
			std::lock_guard<std::mutex> lock(mutex);
			cache.clear();
		});
		return true;
	}();

	const auto key = getCacheKey(connection, path, params);
	const auto now = std::chrono::steady_clock::now();
	{
		std::lock_guard<std::mutex> lock(mutex);
		const auto it = cache.find(key);
		if (it != cache.end() &&
		    now - it->second.time <
			    std::chrono::seconds(cacheTimeoutSeconds)) {
			return it->second.result;
		}
	}

	// Failed requests are cached as well, as they also cost API quota
	auto result = sendYouTubeRequest(connection, method, apiBasePath, path,
					 params, {body});
	std::lock_guard<std::mutex> lock(mutex);
	cache[key] = {result, now};
	return result;
}

YouTubeRequestResult SendYouTubeUploadRequest(YouTubeConnection &connection,
					      const std::string &path,
					      const httplib::Params &params,
					      const std::string &data,
					      const std::string &contentType)
{
	return sendYouTubeRequest(connection, YouTubeRequestMethod::POST,
				  uploadBasePath, path, params,
				  {data, contentType.c_str()});
}

} // namespace advss
