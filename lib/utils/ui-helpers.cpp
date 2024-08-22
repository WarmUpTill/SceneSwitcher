#include "ui-helpers.hpp"
#include "advanced-scene-switcher.hpp"
#include "non-modal-dialog.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <obs-frontend-api.h>
#include <QGraphicsColorizeEffect>
#include <QMainWindow>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWidget>

namespace advss {

QObject *HighlightWidget(QWidget *widget, QColor startColor, QColor endColor,
			 bool once)
{
	QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect(widget);
	widget->setGraphicsEffect(effect);
	QPropertyAnimation *animation =
		new QPropertyAnimation(effect, "color", widget);
	animation->setStartValue(startColor);
	animation->setEndValue(endColor);
	animation->setDuration(1000);

	// Clean up the effect when the animation is deleted
	QWidget::connect(animation, &QPropertyAnimation::destroyed, [widget]() {
		if (widget) {
			widget->setGraphicsEffect(nullptr);
		}
	});

	if (once) {
		animation->start(QPropertyAnimation::DeleteWhenStopped);
		return animation;
	}

	QWidget::connect(animation, &QPropertyAnimation::finished,
			 [animation]() { animation->start(); });
	animation->start();
	return animation;
}

static int getHorizontalScrollBarHeight(QListWidget *list)
{
	if (!list) {
		return 0;
	}
	auto horizontalScrollBar = list->horizontalScrollBar();
	if (!horizontalScrollBar || !horizontalScrollBar->isVisible()) {
		return 0;
	}
	return horizontalScrollBar->height();
}

void SetHeightToContentHeight(QListWidget *list)
{
	auto nrItems = list->count();
	if (nrItems == 0) {
		list->setMaximumHeight(0);
		list->setMinimumHeight(0);
		return;
	}

	int scrollBarHeight = getHorizontalScrollBarHeight(list);
	int height = 2 * list->frameWidth() + scrollBarHeight;

	for (int i = 0; i < nrItems; i++) {
		height += (list->sizeHintForRow(i) + list->spacing());
	}

	list->setMinimumHeight(height);
	list->setMaximumHeight(height);
}

void SetButtonIcon(QAbstractButton *button, const char *path)
{
	QIcon icon;
	icon.addFile(QString::fromUtf8(path), QSize(), QIcon::Normal,
		     QIcon::Off);
	button->setIcon(icon);
}

int FindIdxInRagne(QComboBox *list, int start, int stop,
		   const std::string &value, Qt::MatchFlags flags)
{
	if (value.empty()) {
		return -1;
	}
	auto model = list->model();
	auto startIdx = model->index(start, 0);
	auto match = model->match(startIdx, Qt::DisplayRole,
				  QString::fromStdString(value), 1, flags);
	if (match.isEmpty()) {
		return -1;
	}
	int foundIdx = match.first().row();
	if (foundIdx > stop) {
		return -1;
	}
	return foundIdx;
}

QWidget *GetSettingsWindow()
{
	return SettingsWindowIsOpened() ? AdvSceneSwitcher::window : nullptr;
}

bool DisplayMessage(const QString &msg, bool question, bool modal)
{
	if (!modal) {
		auto dialog = new NonModalMessageDialog(msg, question);
		QMessageBox::StandardButton answer = dialog->ShowMessage();
		return (answer == QMessageBox::Yes);
	} else if (question && modal) {
		auto answer = QMessageBox::question(
			GetSettingsWindow()
				? GetSettingsWindow()
				: static_cast<QMainWindow *>(
					  obs_frontend_get_main_window()),
			obs_module_text("AdvSceneSwitcher.windowTitle"), msg,
			QMessageBox::Yes | QMessageBox::No);
		return answer == QMessageBox::Yes;
	}

	QMessageBox Msgbox;
	Msgbox.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	Msgbox.setText(msg);
	Msgbox.exec();
	return false;
}

void DisplayTrayMessage(const QString &title, const QString &msg,
			const QIcon &icon)
{
	auto tray = reinterpret_cast<QSystemTrayIcon *>(
		obs_frontend_get_system_tray());
	if (!tray) {
		return;
	}
	if (icon.isNull()) {
		tray->showMessage(title, msg);
	} else {
		tray->showMessage(title, msg, icon);
	}
}

std::string GetThemeTypeName()
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	return obs_frontend_is_theme_dark() ? "Dark" : "Light";
#else
	auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) {
		return "Dark";
	}
	QColor color = mainWindow->palette().text().color();
	const bool themeDarkMode = !(color.redF() < 0.5);
	return themeDarkMode ? "Dark" : "Light";
#endif
}

void QeueUITask(void (*task)(void *param), void *param)
{
	obs_queue_task(OBS_TASK_UI, task, param, false);
}

bool IsCursorInWidgetArea(QWidget *widget)
{
	const auto cursorPos = QCursor::pos();
	const auto widgetPos = widget->mapFromGlobal(cursorPos);
	const auto widgetRect = widget->rect();
	return widgetRect.contains(widgetPos);
}

} // namespace advss
