#include "headers/advanced-scene-switcher.hpp"

static QMetaObject::Connection addPulse;

void SceneSwitcher::on_windowAdd_clicked()
{
	ui->windowAdd->disconnect(addPulse);

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->windowSwitches.emplace_back();

	QListWidgetItem *item;
	item = new QListWidgetItem(ui->windowSwitches);
	ui->windowSwitches->addItem(item);
	WindowSwitchWidget *sw =
		new WindowSwitchWidget(&switcher->windowSwitches.back());
	item->setSizeHint(sw->minimumSizeHint());
	ui->windowSwitches->setItemWidget(item, sw);
}

void SceneSwitcher::on_windowRemove_clicked()
{
	QListWidgetItem *item = ui->windowSwitches->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->windowSwitches->currentRow();
		auto &switches = switcher->windowSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void SceneSwitcher::on_windowUp_clicked()
{
	int index = ui->windowSwitches->currentRow();
	if (!listMoveUp(ui->windowSwitches))
		return;

	WindowSwitchWidget *s1 =
		(WindowSwitchWidget *)ui->windowSwitches->itemWidget(
			ui->windowSwitches->item(index));
	WindowSwitchWidget *s2 =
		(WindowSwitchWidget *)ui->windowSwitches->itemWidget(
			ui->windowSwitches->item(index - 1));
	WindowSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->windowSwitches[index],
		  switcher->windowSwitches[index - 1]);
}

void SceneSwitcher::on_windowDown_clicked()
{
	int index = ui->windowSwitches->currentRow();

	if (!listMoveDown(ui->windowSwitches))
		return;

	WindowSwitchWidget *s1 =
		(WindowSwitchWidget *)ui->windowSwitches->itemWidget(
			ui->windowSwitches->item(index));
	WindowSwitchWidget *s2 =
		(WindowSwitchWidget *)ui->windowSwitches->itemWidget(
			ui->windowSwitches->item(index + 1));
	WindowSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->windowSwitches[index],
		  switcher->windowSwitches[index + 1]);
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
	for (WindowSwitch &s : windowSwitches) {
		// True if fullscreen is disabled OR current window is fullscreen
		bool fullscreen = (!s.fullscreen || isFullscreen(s.window));
		// True if maximized is disabled OR current window is maximized
		bool max = (!s.maximized || isMaximized(s.window));
		// True if focus is disabled OR switch is focused
		bool focus = (!s.focus || isFocused(s.window));
		// True if current window is ignored AND switch equals OR matches last window
		bool ignore =
			(ignored &&
			 (title == s.window ||
			  QString::fromStdString(title).contains(
				  QRegularExpression(
					  QString::fromStdString(s.window)))));

		if (isRunning(s.window) &&
		    (fullscreen && (max && (focus || ignore)))) {
			match = true;
			scene = s.scene;
			transition = s.transition;

			if (verbose)
				s.logMatch();
			break;
		}
	}
}

void SwitcherData::saveWindowTitleSwitches(obs_data_t *obj)
{
	obs_data_array_t *windowTitleArray = obs_data_array_create();
	for (WindowSwitch &s : switcher->windowSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if (source && transition) {
			const char *sceneName = obs_source_get_name(source);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "scene", sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_string(array_obj, "window_title",
					    s.window.c_str());
			obs_data_set_bool(array_obj, "fullscreen",
					  s.fullscreen);
			obs_data_set_bool(array_obj, "maximized", s.maximized);
			obs_data_set_bool(array_obj, "focus", s.focus);
			obs_data_array_push_back(windowTitleArray, array_obj);
			obs_source_release(source);
			obs_source_release(transition);
		}
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "switches", windowTitleArray);
	obs_data_array_release(windowTitleArray);

	obs_data_array_t *ignoreWindowsArray = obs_data_array_create();
	for (std::string &window : switcher->ignoreWindowsSwitches) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_string(array_obj, "ignoreWindow", window.c_str());
		obs_data_array_push_back(ignoreWindowsArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "ignoreWindows", ignoreWindowsArray);
	obs_data_array_release(ignoreWindowsArray);
}

void SwitcherData::loadWindowTitleSwitches(obs_data_t *obj)
{
	switcher->windowSwitches.clear();

	obs_data_array_t *windowTitleArray =
		obs_data_get_array(obj, "switches");
	size_t count = obs_data_array_count(windowTitleArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(windowTitleArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		const char *window =
			obs_data_get_string(array_obj, "window_title");
		bool fullscreen = obs_data_get_bool(array_obj, "fullscreen");
#if __APPLE__
		// TODO:
		// not implemented on MacOS as I cannot test it
		bool maximized = false;
#else
		bool maximized = obs_data_get_bool(array_obj, "maximized");
#endif
		bool focus = obs_data_get_bool(array_obj, "focus") ||
			     !obs_data_has_user_value(array_obj, "focus");

		switcher->windowSwitches.emplace_back(
			GetWeakSourceByName(scene), window,
			GetWeakTransitionByName(transition), fullscreen,
			maximized, focus);

		obs_data_release(array_obj);
	}
	obs_data_array_release(windowTitleArray);

	switcher->ignoreWindowsSwitches.clear();

	obs_data_array_t *ignoreWindowsArray =
		obs_data_get_array(obj, "ignoreWindows");
	count = obs_data_array_count(ignoreWindowsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(ignoreWindowsArray, i);

		const char *window =
			obs_data_get_string(array_obj, "ignoreWindow");

		switcher->ignoreWindowsSwitches.emplace_back(window);

		obs_data_release(array_obj);
	}
	obs_data_array_release(ignoreWindowsArray);
}

void SceneSwitcher::setupTitleTab()
{
	for (auto &s : switcher->windowSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->windowSwitches);
		ui->windowSwitches->addItem(item);
		WindowSwitchWidget *sw = new WindowSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->windowSwitches->setItemWidget(item, sw);
	}

	if (switcher->windowSwitches.size() == 0)
		addPulse = PulseWidget(ui->windowAdd, QColor(Qt::green));

	populateWindowSelection(ui->ignoreWindowsWindows);

	for (auto &window : switcher->ignoreWindowsSwitches) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->ignoreWindows);
		item->setData(Qt::UserRole, text);
	}
}

WindowSwitchWidget::WindowSwitchWidget(WindowSwitch *s) : SwitchWidget(s, false)
{
	windows = new QComboBox();
	fullscreen = new QCheckBox("if fullscreen");
	maximized = new QCheckBox("if maximized");
	focused = new QCheckBox("if focused");

	QWidget::connect(windows, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(WindowChanged(const QString &)));
	QWidget::connect(fullscreen, SIGNAL(stateChanged(int)), this,
			 SLOT(FullscreenChanged(int)));
	QWidget::connect(maximized, SIGNAL(stateChanged(int)), this,
			 SLOT(MaximizedChanged(int)));
	QWidget::connect(focused, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusChanged(int)));

	SceneSwitcher::populateWindowSelection(windows);

	windows->setEditable(true);
	windows->setMaxVisibleItems(20);
#if __APPLE__
	// TODO:
	// not implemented on MacOS as I cannot test it
	maximized->setDisabled(true);
	maximized->setVisible(false);
#endif

	if (s) {
		windows->setCurrentText(s->window.c_str());
		fullscreen->setChecked(s->fullscreen);
		maximized->setChecked(s->maximized);
		focused->setChecked(s->focus);
	}

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *mainLayout = new QHBoxLayout;

	mainLayout->addWidget(windows);
	mainLayout->addWidget(scenes);
	mainLayout->addWidget(transitions);
	mainLayout->addWidget(scenes);
	mainLayout->addWidget(fullscreen);
	mainLayout->addWidget(maximized);
	mainLayout->addWidget(focused);
	mainLayout->addStretch();

	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

WindowSwitch *WindowSwitchWidget::getSwitchData()
{
	return switchData;
}

void WindowSwitchWidget::setSwitchData(WindowSwitch *s)
{
	switchData = s;
}

void WindowSwitchWidget::swapSwitchData(WindowSwitchWidget *s1,
					WindowSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	WindowSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void WindowSwitchWidget::WindowChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->window = text.toStdString();
}

void WindowSwitchWidget::FullscreenChanged(int state)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->fullscreen = state;
}

void WindowSwitchWidget::MaximizedChanged(int state)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->maximized = state;
}

void WindowSwitchWidget::FocusChanged(int state)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->focus = state;
}
