#pragma once

#include "export-symbol-helper.hpp"

#include <QColor>
#include <QIcon>
#include <QString>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

class QAbstractButton;
class QComboBox;
class QListWidget;
class QWidget;

namespace advss {

// Returns QObject* to QPropertyAnimation object
// Delete it to stop the animation
EXPORT QObject *HighlightWidget(QWidget *widget, QColor startColor,
				QColor endColor = QColor(0, 0, 0, 0),
				bool once = false);

EXPORT void SetHeightToContentHeight(QListWidget *list);
EXPORT void SetButtonIcon(QAbstractButton *button, const char *path);

EXPORT int
FindIdxInRagne(QComboBox *list, int start, int stop, const std::string &value,
	       Qt::MatchFlags = Qt::MatchExactly | Qt::MatchCaseSensitive);
EXPORT void SetRowVisibleByValue(QComboBox *list, const QString &value,
				 bool show);

EXPORT bool DisplayMessage(const QString &msg, bool question = false,
			   bool modal = true);
EXPORT void DisplayTrayMessage(const QString &title, const QString &msg,
			       const QIcon &icon = QIcon());

EXPORT std::string GetThemeTypeName();
EXPORT QWidget *GetSettingsWindow();

EXPORT void QueueUITaskRaw(void (*task)(void *param), void *param,
			   bool wait = false);

// Runs func on the main/UI thread; blocks if wait is true.
template<typename F> void QueueUITask(F &&func, bool wait = false)
{
	using FnType = std::decay_t<F>;
	auto *heapFunc = new FnType(std::forward<F>(func));

	QueueUITaskRaw(
		[](void *param) {
			std::unique_ptr<FnType> fn(
				static_cast<FnType *>(param));
			(*fn)();
		},
		heapFunc, wait);
}

bool IsCursorInWidgetArea(QWidget *widget);

} // namespace advss
