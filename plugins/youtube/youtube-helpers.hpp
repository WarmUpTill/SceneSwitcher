#pragma once
#include "youtube-oauth.hpp"

#include <httplib.h>
#include <obs.hpp>

namespace advss {

class YouTubeConnection;

struct YouTubeRequestResult {
	int status = 0;
	OBSData data = nullptr;
	std::string errorReason;
};

enum class YouTubeRequestMethod {
	GET,
	POST,
};

TokenResponse ExchangeAuthCode(const OAuthClientConfig &config,
			       const std::string &code,
			       const std::string &codeVerifier,
			       const std::string &redirectUri);
TokenResponse RefreshAccessToken(const OAuthClientConfig &config,
				 const std::string &refreshToken);
void RevokeToken(const std::string &token);

YouTubeRequestResult
SendYouTubeRequest(YouTubeConnection &connection, YouTubeRequestMethod method,
		   const std::string &path, const httplib::Params &params = {},
		   const std::string &body = "", bool useCache = false);

} // namespace advss
