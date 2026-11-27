#include "catch.hpp"

#include <youtube-oauth.hpp>

#include <QUrl>
#include <QUrlQuery>

TEST_CASE("PKCE code challenge", "[youtube-oauth]")
{
	// Test vector from RFC 7636 Appendix B
	REQUIRE(advss::GetCodeChallenge(
			"dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk") ==
		"E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
}

TEST_CASE("PKCE code verifier", "[youtube-oauth]")
{
	const auto verifier = advss::GenerateCodeVerifier();
	REQUIRE(verifier.size() >= 43);
	REQUIRE(verifier.size() <= 128);
	REQUIRE(verifier.find_first_not_of(
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~") ==
		std::string::npos);
	REQUIRE(verifier != advss::GenerateCodeVerifier());
}

TEST_CASE("Auth URL", "[youtube-oauth]")
{
	advss::OAuthClientConfig config{"id+with spaces", "secret"};
	const auto url = QUrl(QString::fromStdString(advss::BuildAuthUrl(
		config, "http://127.0.0.1:1234", "challenge", "state")));

	REQUIRE(url.host() == "accounts.google.com");
	REQUIRE(url.path() == "/o/oauth2/v2/auth");

	const QUrlQuery query(url);
	const auto value = [&query](const char *key) {
		return query.queryItemValue(key, QUrl::FullyDecoded)
			.toStdString();
	};
	REQUIRE(value("client_id") == "id+with spaces");
	REQUIRE(value("redirect_uri") == "http://127.0.0.1:1234");
	REQUIRE(value("response_type") == "code");
	REQUIRE(value("scope") == "https://www.googleapis.com/auth/youtube");
	REQUIRE(value("access_type") == "offline");
	REQUIRE(value("prompt") == "consent");
	REQUIRE(value("code_challenge") == "challenge");
	REQUIRE(value("code_challenge_method") == "S256");
	REQUIRE(value("state") == "state");
	REQUIRE_FALSE(query.hasQueryItem("client_secret"));
}

TEST_CASE("Token response parsing", "[youtube-oauth]")
{
	SECTION("Success")
	{
		const auto before = std::chrono::system_clock::now();
		const auto result = advss::ParseTokenResponse(
			R"({"access_token":"a","refresh_token":"r","expires_in":3599,"token_type":"Bearer"})");
		REQUIRE(result.error.empty());
		REQUIRE(result.accessToken == "a");
		REQUIRE(result.refreshToken == "r");
		REQUIRE(result.expiresAt >=
			before + std::chrono::seconds(3599));
	}
	SECTION("Refresh without new refresh token")
	{
		const auto result = advss::ParseTokenResponse(
			R"({"access_token":"a","expires_in":3599})");
		REQUIRE(result.error.empty());
		REQUIRE(result.refreshToken.empty());
	}
	SECTION("Error")
	{
		const auto result = advss::ParseTokenResponse(
			R"({"error":"invalid_grant","error_description":"Token has been expired or revoked."})");
		REQUIRE(result.error == "invalid_grant");
		REQUIRE(result.errorDescription ==
			"Token has been expired or revoked.");
	}
	SECTION("Invalid")
	{
		REQUIRE(advss::ParseTokenResponse("not json").error ==
			"invalid_response");
		REQUIRE(advss::ParseTokenResponse("{}").error ==
			"invalid_response");
	}
}
