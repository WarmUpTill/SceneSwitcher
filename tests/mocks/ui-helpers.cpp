#include "ui-helpers.hpp"

class QObejct;

namespace advss {

QObject *HighlightWidget(QWidget *widget, QColor startColor, QColor endColor,
			 bool once)
{
	return nullptr;
}

void SetHeightToContentHeight(QListWidget *list) {}

void SetButtonIcon(QAbstractButton *button, const char *path) {}

int FindIdxInRagne(QComboBox *list, int start, int stop,
		   const std::string &value, Qt::MatchFlags flags)
{
	return -1;
}

void SetRowMatchingValueVisible(QComboBox *list, const QString &value,
				bool show)
{
}

bool DisplayMessage(const QString &msg, bool question, bool modal)
{
	return false;
}

void DisplayTrayMessage(const QString &title, const QString &msg,
			const QIcon &icon)
{
}

std::string GetThemeTypeName()
{
	return "Dark";
}

void QeueUITask(void (*task)(void *param), void *) {}

QWidget *GetSettingsWindow()
{
	return nullptr;
}

} // namespace advss
