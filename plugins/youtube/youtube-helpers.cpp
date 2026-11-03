#include "youtube-helpers.hpp"
#include "youtube-connection.hpp"

#include <log-helper.hpp>

namespace advss {

static const char *tokenHost = "https://oauth2.googleapis.com";
static const char *apiHost = "https://www.googleapis.com";
static const char *apiBasePath = "/youtube/v3";

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

static std::string getErrorReason(obs_data_t *data)
{
	OBSDataAutoRelease error = obs_data_get_obj(data, "error");
	OBSDataArrayAutoRelease errors = obs_data_get_array(error, "errors");
	OBSDataAutoRelease firstError = obs_data_array_item(errors, 0);
	return obs_data_get_string(firstError, "reason");
}

static httplib::Result sendRequest(YouTubeRequestMethod method,
				   const std::string &path,
				   const httplib::Params &params,
				   const std::string &body,
				   const std::string &accessToken)
{
	httplib::Client cli(apiHost);
	const httplib::Headers headers{
		{"Authorization", "Bearer " + accessToken}};
	const auto fullPath =
		httplib::append_query_params(apiBasePath + path, params);

	switch (method) {
	case YouTubeRequestMethod::GET:
		vblog(LOG_INFO, "YouTube GET request to %s", fullPath.c_str());
		return cli.Get(fullPath, headers);
	case YouTubeRequestMethod::POST:
		vblog(LOG_INFO, "YouTube POST request to %s", fullPath.c_str());
		return cli.Post(fullPath, headers, body, "application/json");
	}
	return httplib::Result();
}

YouTubeRequestResult SendYouTubeRequest(YouTubeConnection &connection,
					YouTubeRequestMethod method,
					const std::string &path,
					const httplib::Params &params,
					const std::string &body)
{
	auto accessToken = connection.GetAccessToken();
	if (!accessToken) {
		return {};
	}

	auto response = sendRequest(method, path, params, body, *accessToken);
	if (response && response->status == 401) {
		accessToken = connection.GetAccessToken(true);
		if (!accessToken) {
			return {};
		}
		response =
			sendRequest(method, path, params, body, *accessToken);
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
	return result;
}

} // namespace advss
