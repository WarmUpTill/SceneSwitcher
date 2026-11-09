#include "youtube-oauth.hpp"

#include <nlohmann/json.hpp>
#include <QByteArray>
#include <QCryptographicHash>
#include <QUrl>
#include <random>
#include <utility>

namespace advss {

static const char *authEndpoint =
	"https://accounts.google.com/o/oauth2/v2/auth";
static const char *youtubeScope = "https://www.googleapis.com/auth/youtube";

std::optional<OAuthClientConfig> GetBuiltInClientConfig()
{
	return {};
}

static std::string generateRandomString(size_t length)
{
	static const std::string chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<size_t> dis(0, chars.size() - 1);

	std::string result;
	result.reserve(length);
	for (size_t i = 0; i < length; ++i) {
		result += chars[dis(gen)];
	}
	return result;
}

std::string GenerateCodeVerifier()
{
	// RFC 7636 requires 43 to 128 characters
	return generateRandomString(64);
}

std::string GetCodeChallenge(const std::string &verifier)
{
	const auto hash =
		QCryptographicHash::hash(QByteArray::fromStdString(verifier),
					 QCryptographicHash::Sha256);
	return hash
		.toBase64(QByteArray::Base64UrlEncoding |
			  QByteArray::OmitTrailingEquals)
		.toStdString();
}

std::string GenerateState()
{
	return generateRandomString(32);
}

std::string BuildAuthUrl(const OAuthClientConfig &config,
			 const std::string &redirectUri,
			 const std::string &codeChallenge,
			 const std::string &state)
{
	const std::pair<const char *, std::string> params[] = {
		{"client_id", config.clientId},
		{"redirect_uri", redirectUri},
		{"response_type", "code"},
		{"scope", youtubeScope},
		{"access_type", "offline"},
		// Without consent Google does not return a new refresh token
		// when the user had already granted access before
		{"prompt", "consent"},
		{"code_challenge", codeChallenge},
		{"code_challenge_method", "S256"},
		{"state", state},
	};

	// QUrlQuery is not used as it does not encode '+'
	std::string url = authEndpoint;
	char separator = '?';
	for (const auto &[key, value] : params) {
		url += separator;
		url += key;
		url += '=';
		url += QUrl::toPercentEncoding(QString::fromStdString(value))
			       .toStdString();
		separator = '&';
	}
	return url;
}

TokenResponse ParseTokenResponse(const std::string &body)
{
	TokenResponse result;
	try {
		const auto json = nlohmann::json::parse(body);
		result.accessToken = json.value("access_token", "");
		result.refreshToken = json.value("refresh_token", "");
		result.error = json.value("error", "");
		result.errorDescription = json.value("error_description", "");
		const auto expiresIn = json.value("expires_in", 0);
		result.expiresAt = std::chrono::system_clock::now() +
				   std::chrono::seconds(expiresIn);
	} catch (const nlohmann::json::exception &) {
		result.error = "invalid_response";
	}

	if (result.error.empty() && result.accessToken.empty()) {
		result.error = "invalid_response";
	}
	return result;
}

} // namespace advss
