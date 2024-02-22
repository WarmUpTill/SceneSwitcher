#pragma once
#include "export-symbol-helper.hpp"

#include <optional>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QString>
#include <QTextStream>
#include <QWidget>
#include <string>

namespace advss {

EXPORT std::pair<int, int> GetCursorPos();

EXPORT bool DoubleEquals(double left, double right, double epsilon);

bool ReplaceAll(std::string &str, const std::string &from,
		const std::string &to);
EXPORT std::optional<std::string> GetJsonField(const std::string &json,
					       const std::string &id);
EXPORT bool CompareIgnoringLineEnding(QString &s1, QString &s2);
std::string ToString(double value);

EXPORT std::string GetDataFilePath(const std::string &file);
QString GetDefaultSettingsSaveLocation();

/* Legacy helpers */

void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QPushButton *addButton = nullptr,
		    QMetaObject::Connection *addHighlight = nullptr);
bool listMoveUp(QListWidget *list);
bool listMoveDown(QListWidget *list);

} // namespace advss
