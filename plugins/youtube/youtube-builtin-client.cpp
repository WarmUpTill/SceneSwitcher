#include "youtube-oauth.hpp"
#include "youtube-client-config.hpp"

namespace advss {

std::optional<OAuthClientConfig> GetBuiltInClientConfig()
{
	static const std::string clientId = ADVSS_YOUTUBE_CLIENT_ID;
	static const std::string clientSecret = ADVSS_YOUTUBE_CLIENT_SECRET;
	if (clientId.empty() || clientSecret.empty()) {
		return {};
	}
	return OAuthClientConfig{clientId, clientSecret};
}

} // namespace advss
