#pragma once
#include "export-symbol-helper.hpp"

#include <optional>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <string>

namespace advss {

EXPORT std::pair<int, int> GetCursorPos();

bool ReplaceAll(std::string &str, const std::string &from,
		const std::string &to);
EXPORT std::optional<std::string> GetJsonField(const std::string &json,
					       const std::string &id);
EXPORT bool CompareIgnoringLineEnding(QString &s1, QString &s2);
std::string ToString(double value);

/* Legacy helpers */

void listAddClicked(QListWidget *list, QWidget *newWidget,
		    QPushButton *addButton = nullptr,
		    QObject *addHighlight = nullptr);
bool listMoveUp(QListWidget *list);
bool listMoveDown(QListWidget *list);

} // namespace advss
