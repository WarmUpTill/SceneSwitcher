#include "source-settings-helpers.hpp"
#include "log-helper.hpp"
#include "json-helpers.hpp"

namespace advss {

std::optional<std::string> GetSourceSettings(OBSWeakSource ws,
					     bool includeDefaults)
{
	if (!ws) {
		return {};
	}

	OBSSourceAutoRelease source = obs_weak_source_get_source(ws);
	OBSDataAutoRelease dataWithoutDefaults =
		obs_source_get_settings(source);
	if (!dataWithoutDefaults) {
		return {};
	}

	OBSDataAutoRelease dataWithDefaults =
		obs_data_get_defaults(dataWithoutDefaults);
	obs_data_apply(dataWithDefaults, dataWithoutDefaults);

	auto &data = includeDefaults ? dataWithDefaults : dataWithoutDefaults;
	auto json = obs_data_get_json(data);
	if (!json) {
		return {};
	}
	return json;
}

void SetSourceSettings(obs_source_t *s, const std::string &settings)
{
	if (settings.empty()) {
		return;
	}

	obs_data_t *data = obs_data_create_from_json(settings.c_str());
	if (!data) {
		blog(LOG_WARNING, "invalid source settings provided: \n%s",
		     settings.c_str());
		return;
	}
	obs_source_update(s, data);
	obs_data_release(data);
}

bool CompareSourceSettings(const std::string &sourceSettings,
			   const std::string &settings,
			   const RegexConfig &regex)
{
	return MatchJson(sourceSettings, settings, regex);
}

} // namespace advss
