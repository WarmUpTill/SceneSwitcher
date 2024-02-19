#include "utility.hpp"

#include <cmath>

namespace advss {

std::pair<int, int> GetCursorPos()
{
	return {0, 0};
}

bool DoubleEquals(double left, double right, double epsilon)
{
	return (fabs(left - right) < epsilon);
}

bool ReplaceAll(std::string &str, const std::string &from,
		const std::string &to)
{
	if (from.empty()) {
		return false;
	}
	size_t start_pos = 0;
	bool somethingWasReplaced = false;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
		somethingWasReplaced = true;
	}
	return somethingWasReplaced;
}

std::optional<std::string> GetJsonField(const std::string &jsonStr,
					const std::string &fieldToExtract)
{
	return {};
}

bool CompareIgnoringLineEnding(QString &s1, QString &s2)
{
	// Let QT deal with different types of lineendings
	QTextStream s1stream(&s1);
	QTextStream s2stream(&s2);

	while (!s1stream.atEnd() || !s2stream.atEnd()) {
		QString s1s = s1stream.readLine();
		QString s2s = s2stream.readLine();
		if (s1s != s2s) {
			return false;
		}
	}

	if (!s1stream.atEnd() && !s2stream.atEnd()) {
		return false;
	}

	return true;
}

std::string GetDataFilePath(const std::string &file)
{
	return "";
}

QString GetDefaultSettingsSaveLocation()
{
	return "";
}

void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QPushButton *addButton,
		    QMetaObject::Connection *addHighlight)
{
}

bool listMoveUp(QListWidget *list)
{
	return false;
}

bool listMoveDown(QListWidget *list)
{
	return false;
}

} // namespace advss
