#pragma once
#include "export-symbol-helper.hpp"
#include "regex-config.hpp"

#include <QString>
#include <string>
#include <utility>
#include <vector>

namespace advss {

EXPORT QString FormatJsonString(std::string);
EXPORT QString FormatJsonString(QString);
EXPORT bool MatchJson(const std::string &json1, const std::string &json2,
		      const RegexConfig &regex);
EXPORT std::optional<std::string> GetJsonField(const std::string &json,
					       const std::string &id);
EXPORT std::optional<std::string> QueryJson(const std::string &json,
					    const std::string &query);
EXPORT std::optional<std::string> AccessJsonArrayIndex(const std::string &json,
						       const int index);
EXPORT std::optional<std::string>
ExtractSingleJsonArrayElement(const std::string &json);

EXPORT std::vector<std::pair<std::string, int>>
CollectDistinctJsonStringValues(const std::string &json);
EXPORT std::string ReplaceJsonStringValue(const std::string &json,
					  const std::string &oldValue,
					  const std::string &newValue);
EXPORT std::string GenerateImportPlaceholder();

} // namespace advss
