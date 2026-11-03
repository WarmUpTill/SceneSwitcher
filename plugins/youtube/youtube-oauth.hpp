#pragma once
#include <chrono>
#include <optional>
#include <string>

namespace advss {

struct OAuthClientConfig {
	std::string clientId;
	std::string clientSecret;
};

struct TokenResponse {
	std::string accessToken;
	std::string refreshToken;
	std::chrono::system_clock::time_point expiresAt;
	std::string error;
	std::string errorDescription;
};

std::optional<OAuthClientConfig> GetBuiltInClientConfig();

std::string GenerateCodeVerifier();
std::string GetCodeChallenge(const std::string &verifier);
std::string GenerateState();
std::string BuildAuthUrl(const OAuthClientConfig &config,
			 const std::string &redirectUri,
			 const std::string &codeChallenge,
			 const std::string &state);
TokenResponse ParseTokenResponse(const std::string &body);

} // namespace advss
