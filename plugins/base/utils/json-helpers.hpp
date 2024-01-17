#pragma once
#include <QString>
#include <string>
#include <regex-config.hpp>

namespace advss {

QString FormatJsonString(std::string);
QString FormatJsonString(QString);
bool MatchJson(const std::string &json1, const std::string &json2,
	       const RegexConfig &regex);

} // namespace advss
