#pragma once
#include "export-symbol-helper.hpp"

#include <QColor>
#include <QComboBox>
#include <QIcon>
#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <string>

namespace advss {

// Returns QObject* to QPropertyAnimation object
// Delete it to stop the animation
EXPORT QObject *HighlightWidget(QWidget *widget, QColor startColor,
				QColor endColor = QColor(0, 0, 0, 0),
				bool once = false);

EXPORT void SetHeightToContentHeight(QListWidget *list);
EXPORT void SetButtonIcon(QPushButton *button, const char *path);
EXPORT int
FindIdxInRagne(QComboBox *list, int start, int stop, const std::string &value,
	       Qt::MatchFlags = Qt::MatchExactly | Qt::MatchCaseSensitive);

EXPORT bool DisplayMessage(const QString &msg, bool question = false,
			   bool modal = true);
EXPORT void DisplayTrayMessage(const QString &title, const QString &msg,
			       const QIcon &icon = QIcon());

EXPORT std::string GetThemeTypeName();

void QeueUITask(void (*task)(void *param), void *param);

} // namespace advss
