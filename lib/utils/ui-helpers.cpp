#include "ui-helpers.hpp"
#include "non-modal-dialog.hpp"
#include "obs-module-helper.hpp"

#include <obs-frontend-api.h>
#include <QGraphicsColorizeEffect>
#include <QMainWindow>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWidget>

namespace advss {

QMetaObject::Connection PulseWidget(QWidget *widget, QColor startColor,
				    QColor endColor, bool once)
{
	QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect(widget);
	widget->setGraphicsEffect(effect);
	QPropertyAnimation *animation =
		new QPropertyAnimation(effect, "color", widget);
	animation->setStartValue(startColor);
	animation->setEndValue(endColor);
	animation->setDuration(1000);

	QMetaObject::Connection con;
	if (once) {
		auto widgetPtr = widget;
		con = QWidget::connect(
			animation, &QPropertyAnimation::finished,
			[widgetPtr]() {
				if (widgetPtr) {
					widgetPtr->setGraphicsEffect(nullptr);
				}
			});
		animation->start(QPropertyAnimation::DeleteWhenStopped);
	} else {
		auto widgetPtr = widget;
		con = QWidget::connect(
			animation, &QPropertyAnimation::finished,
			[animation, widgetPtr]() {
				QTimer *timer = new QTimer(widgetPtr);
				QWidget::connect(timer, &QTimer::timeout,
						 [animation] {
							 animation->start();
						 });
				timer->setSingleShot(true);
				timer->start(1000);
			});
		animation->start();
	}
	return con;
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
	int height = (list->sizeHintForRow(0) + list->spacing()) * nrItems +
		     2 * list->frameWidth() + scrollBarHeight;
	list->setMinimumHeight(height);
	list->setMaximumHeight(height);
}

void SetButtonIcon(QPushButton *button, const char *path)
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

QWidget *GetSettingsWindow();

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

} // namespace advss
