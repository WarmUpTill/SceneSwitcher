#include "json-helpers.hpp"

#ifdef JSONPATH_SUPPORT
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>
#endif
#include <nlohmann/json.hpp>
#include <QJsonDocument>
#include <unordered_map>
#include <util/bmem.h>
#include <util/platform.h>

namespace advss {

QString FormatJsonString(std::string s)
{
	return FormatJsonString(QString::fromStdString(s));
}

QString FormatJsonString(QString json)
{
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
	if (doc.isNull()) {
		return "";
	}
	return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

bool MatchJson(const std::string &json1, const std::string &json2,
	       const RegexConfig &regex)
{
	auto j1 = FormatJsonString(json1).toStdString();
	auto j2 = FormatJsonString(json2).toStdString();

	if (j1.empty()) {
		j1 = json1;
	}
	if (j2.empty()) {
		j2 = json2;
	}

	if (regex.Enabled()) {
		return regex.Matches(j1, j2);
	}
	return j1 == j2;
}

std::optional<std::string> GetJsonField(const std::string &jsonStr,
					const std::string &fieldToExtract)
{
	try {
		nlohmann::json json = nlohmann::json::parse(jsonStr);
		auto it = json.find(fieldToExtract);
		if (it == json.end()) {
			return {};
		}
		if (it->is_string()) {
			return it->get<std::string>();
		}
		return it->dump();
	} catch (const nlohmann::json::exception &) {
		return {};
	}
	return {};
}

std::optional<std::string> QueryJson(const std::string &jsonStr,
				     const std::string &query)
{
#ifdef JSONPATH_SUPPORT
	try {
		auto json = jsoncons::json::parse(jsonStr);
		auto result = jsoncons::jsonpath::json_query(json, query);
		return result.as_string();
	} catch (const jsoncons::json_exception &) {
		return {};
	} catch (const jsoncons::json_errc &) {
		return {};
	}
#else
	return {};
#endif
}

std::optional<std::string> AccessJsonArrayIndex(const std::string &jsonStr,
						const int index)
{
	try {
		nlohmann::json json = nlohmann::json::parse(jsonStr);
		if (!json.is_array() || index >= (int)json.size() ||
		    index < 0) {
			return {};
		}
		auto result = json.at(index);
		if (result.is_string()) {
			return result.get<std::string>();
		}
		return result.dump();
	} catch (const nlohmann::json::exception &) {
		return {};
	}
	return {};
}

std::optional<std::string>
ExtractSingleJsonArrayElement(const std::string &jsonStr)
{
	try {
		nlohmann::json json = nlohmann::json::parse(jsonStr);
		if (!json.is_array() || json.size() != 1) {
			return {};
		}
		auto result = json.at(0);
		if (result.is_string()) {
			return result.get<std::string>();
		}
		return result.dump();
	} catch (const nlohmann::json::exception &) {
		return {};
	}
	return {};
}

static void collectStrings(const nlohmann::json &json,
			   std::vector<std::pair<std::string, int>> &result,
			   std::unordered_map<std::string, size_t> &index)
{
	if (json.is_string()) {
		auto value = json.get<std::string>();
		auto it = index.find(value);
		if (it == index.end()) {
			index[value] = result.size();
			result.emplace_back(value, 1);
		} else {
			result[it->second].second++;
		}
		return;
	}
	if (json.is_object()) {
		for (const auto &item : json.items()) {
			collectStrings(item.value(), result, index);
		}
		return;
	}
	if (json.is_array()) {
		for (const auto &value : json) {
			collectStrings(value, result, index);
		}
	}
}

std::vector<std::pair<std::string, int>>
CollectDistinctJsonStringValues(const std::string &jsonStr)
{
	std::vector<std::pair<std::string, int>> result;
	try {
		nlohmann::json json = nlohmann::json::parse(jsonStr);
		std::unordered_map<std::string, size_t> index;
		collectStrings(json, result, index);
	} catch (const nlohmann::json::exception &) {
	}
	return result;
}

static void replaceStrings(nlohmann::json &json, const std::string &oldValue,
			   const std::string &newValue)
{
	if (json.is_string()) {
		if (json.get<std::string>() == oldValue) {
			json = newValue;
		}
		return;
	}
	if (json.is_object()) {
		for (auto &item : json.items()) {
			replaceStrings(item.value(), oldValue, newValue);
		}
		return;
	}
	if (json.is_array()) {
		for (auto &value : json) {
			replaceStrings(value, oldValue, newValue);
		}
	}
}

std::string ReplaceJsonStringValue(const std::string &jsonStr,
				   const std::string &oldValue,
				   const std::string &newValue)
{
	try {
		nlohmann::json json = nlohmann::json::parse(jsonStr);
		replaceStrings(json, oldValue, newValue);
		return json.dump();
	} catch (const nlohmann::json::exception &) {
		return jsonStr;
	}
}

std::string GenerateImportPlaceholder()
{
	char *uuid = os_generate_uuid();
	std::string result =
		"ADVSS_IMPORT_PLACEHOLDER_" + std::string(uuid ? uuid : "");
	bfree(uuid);
	return result;
}

} // namespace advss
