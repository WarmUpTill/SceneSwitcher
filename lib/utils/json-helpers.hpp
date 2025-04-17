#pragma once
#include "export-symbol-helper.hpp"
#include "regex-config.hpp"

#include <QString>
#include <string>

namespace advss {

EXPORT QString FormatJsonString(std::string);
EXPORT QString FormatJsonString(QString);
EXPORT bool MatchJson(const std::string &json1, const std::string &json2,
		      const RegexConfig &regex);
EXPORT std::optional<std::string> GetJsonField(const std::string &json,
					       const std::string &id);

} // namespace advss
