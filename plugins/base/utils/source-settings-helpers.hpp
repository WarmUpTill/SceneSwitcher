#pragma once
#include <regex-config.hpp>

#include <obs.hpp>

#include <optional>
#include <string>

namespace advss {

std::optional<std::string> GetSourceSettings(OBSWeakSource ws,
					     bool includeDefaults);
void SetSourceSettings(obs_source_t *s, const std::string &settings);
bool CompareSourceSettings(const std::string &sourceSettings,
			   const std::string &settings,
			   const RegexConfig &regex);

} // namespace advss
