#include <obs-module.h>
#include "headers/advanced-scene-switcher.hpp"

bool isRunning(std::string &title)
{
	QStringList windows;

	GetWindowList(windows);
	// True if switch is running (direct)
	bool equals = windows.contains(QString::fromStdString(title));
	// True if switch is running (regex)
	bool matches = (windows.indexOf(QRegularExpression(
				QString::fromStdString(title))) != -1);

	return (equals || matches);
}

bool isFocused(std::string &title)
{
	std::string current;

	GetCurrentWindowTitle(current);
	// True if switch equals current window
	bool equals = (title == current);
	// True if switch matches current window
	bool matches = QString::fromStdString(current).contains(
		QRegularExpression(QString::fromStdString(title)));

	return (equals || matches);
}

void SceneSwitcher::on_up_clicked()
{
	int index = ui->switches->currentRow();
	if (index != -1 && index != 0) {
		ui->switches->insertItem(index - 1,
					 ui->switches->takeItem(index));
		ui->switches->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->windowSwitches.begin() + index,
			  switcher->windowSwitches.begin() + index - 1);
	}
}

void SceneSwitcher::on_down_clicked()
{
	int index = ui->switches->currentRow();
	if (index != -1 && index != ui->switches->count() - 1) {
		ui->switches->insertItem(index + 1,
					 ui->switches->takeItem(index));
		ui->switches->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->windowSwitches.begin() + index,
			  switcher->windowSwitches.begin() + index + 1);
	}
}

void SceneSwitcher::on_add_clicked()
{
	QString sceneName = ui->scenes->currentText();
	QString windowName = ui->windows->currentText();
	QString transitionName = ui->transitions->currentText();
	bool fullscreen = ui->fullscreenCheckBox->isChecked();
	bool focus = ui->focusCheckBox->isChecked();

	if (windowName.isEmpty() || sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);
	QVariant v = QVariant::fromValue(windowName);

	QString text = MakeSwitchName(sceneName, windowName, transitionName,
				      fullscreen, focus);

	int idx = FindByData(windowName);

	if (idx == -1) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->windowSwitches.emplace_back(
			source, windowName.toUtf8().constData(), transition,
			fullscreen, focus);

		QListWidgetItem *item = new QListWidgetItem(text, ui->switches);
		item->setData(Qt::UserRole, v);
	} else {
		QListWidgetItem *item = ui->switches->item(idx);
		item->setText(text);

		std::string window = windowName.toUtf8().constData();

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->windowSwitches) {
				if (s.window == window) {
					s.scene = source;
					s.transition = transition;
					s.fullscreen = fullscreen;
					s.focus = focus;
					break;
				}
			}
		}
	}
}

void SceneSwitcher::on_remove_clicked()
{
	QListWidgetItem *item = ui->switches->currentItem();
	if (!item)
		return;

	std::string window =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->windowSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.window == window) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_ignoreWindowsAdd_clicked()
{
	QString windowName = ui->ignoreWindowsWindows->currentText();

	if (windowName.isEmpty())
		return;

	QVariant v = QVariant::fromValue(windowName);

	QList<QListWidgetItem *> items =
		ui->ignoreWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item =
			new QListWidgetItem(windowName, ui->ignoreWindows);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->ignoreWindowsSwitches.emplace_back(
			windowName.toUtf8().constData());
	}
}

void SceneSwitcher::on_ignoreWindowsRemove_clicked()
{
	QListWidgetItem *item = ui->ignoreWindows->currentItem();
	if (!item)
		return;

	QString windowName = item->data(Qt::UserRole).toString();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->ignoreWindowsSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s == windowName.toUtf8().constData()) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

int SceneSwitcher::FindByData(const QString &window)
{
	int count = ui->switches->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->switches->item(i);
		QString itemWindow = item->data(Qt::UserRole).toString();

		if (itemWindow == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::IgnoreWindowsFindByData(const QString &window)
{
	int count = ui->ignoreWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->ignoreWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

void SceneSwitcher::on_switches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->switches->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->windowSwitches) {
		if (window.compare(s.window.c_str()) == 0) {
			std::string name = GetWeakSourceName(s.scene);
			std::string transitionName =
				GetWeakSourceName(s.transition);
			ui->scenes->setCurrentText(name.c_str());
			ui->windows->setCurrentText(window);
			ui->transitions->setCurrentText(transitionName.c_str());
			ui->fullscreenCheckBox->setChecked(s.fullscreen);
			ui->focusCheckBox->setChecked(s.focus);
			break;
		}
	}
}

void SceneSwitcher::on_ignoreWindows_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->ignoreWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->ignoreWindowsSwitches) {
		if (window.compare(s.c_str()) == 0) {
			ui->ignoreWindowsWindows->setCurrentText(s.c_str());
			break;
		}
	}
}

void SwitcherData::checkWindowTitleSwitch(bool &match, OBSWeakSource &scene,
					  OBSWeakSource &transition)
{
	std::string title;
	bool ignored = false;

	// Check if current window is ignored
	GetCurrentWindowTitle(title);
	for (auto &window : ignoreWindowsSwitches) {
		// True if ignored switch equals current window
		bool equals = (title == window);
		// True if ignored switch matches current window
		bool matches = QString::fromStdString(title).contains(
			QRegularExpression(QString::fromStdString(window)));

		if (equals || matches) {
			ignored = true;
			title = lastTitle;

			break;
		}
	}
	lastTitle = title;

	// Check for match
	for (WindowSceneSwitch &s : windowSwitches) {
		// True if fullscreen is disabled OR current window is fullscreen
		bool fullscreen = (!s.fullscreen || isFullscreen(s.window));
		// True if focus is disabled OR switch is focused
		bool focus = (!s.focus || isFocused(s.window));
		// True if current window is ignored AND switch equals OR matches last window
		bool ignore =
			(ignored &&
			 (title == s.window ||
			  QString::fromStdString(title).contains(
				  QRegularExpression(
					  QString::fromStdString(s.window)))));

		if (isRunning(s.window) && (fullscreen && (focus || ignore))) {
			match = true;
			scene = s.scene;
			transition = s.transition;

			break;
		}
	}
}
