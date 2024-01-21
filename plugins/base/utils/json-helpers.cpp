#include "json-helpers.hpp"

#include <QJsonDocument>

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

} // namespace advss
