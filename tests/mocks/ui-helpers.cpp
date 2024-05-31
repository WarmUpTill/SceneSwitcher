#include "ui-helpers.hpp"

namespace advss {

QMetaObject::Connection PulseWidget(QWidget *widget, QColor startColor,
				    QColor endColor, bool once)
{
	return {};
}

void SetHeightToContentHeight(QListWidget *list) {}

void SetButtonIcon(QPushButton *button, const char *path) {}

int FindIdxInRagne(QComboBox *list, int start, int stop,
		   const std::string &value, Qt::MatchFlags flags)
{
	return -1;
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
