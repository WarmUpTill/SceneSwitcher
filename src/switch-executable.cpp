#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_executableAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->executableSwitches.emplace_back();

	listAddClicked(ui->executables,
		       new ExecutableSwitchWidget(
			       &switcher->executableSwitches.back()),
		       ui->executableAdd, &addPulse);
}

void AdvSceneSwitcher::on_executableRemove_clicked()
{
	QListWidgetItem *item = ui->executables->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->executables->currentRow();
		auto &switches = switcher->executableSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_executableUp_clicked()
{
	int index = ui->executables->currentRow();
	if (!listMoveUp(ui->executables))
		return;

	ExecutableSwitchWidget *s1 =
		(ExecutableSwitchWidget *)ui->executables->itemWidget(
			ui->executables->item(index));
	ExecutableSwitchWidget *s2 =
		(ExecutableSwitchWidget *)ui->executables->itemWidget(
			ui->executables->item(index - 1));
	ExecutableSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->executableSwitches[index],
		  switcher->executableSwitches[index - 1]);
}

void AdvSceneSwitcher::on_executableDown_clicked()
{
	int index = ui->executables->currentRow();

	if (!listMoveDown(ui->executables))
		return;

	ExecutableSwitchWidget *s1 =
		(ExecutableSwitchWidget *)ui->executables->itemWidget(
			ui->executables->item(index));
	ExecutableSwitchWidget *s2 =
		(ExecutableSwitchWidget *)ui->executables->itemWidget(
			ui->executables->item(index + 1));
	ExecutableSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->executableSwitches[index],
		  switcher->executableSwitches[index + 1]);
}

void SwitcherData::checkExeSwitch(bool &match, OBSWeakSource &scene,
				  OBSWeakSource &transition)
{
	std::string title;
	QStringList runningProcesses;
	bool ignored = false;

	// Check if current window is ignored
	GetCurrentWindowTitle(title);
	for (auto &window : ignoreWindowsSwitches) {
		// True if ignored switch equals title
		bool equals = (title == window);
		// True if ignored switch matches title
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
	GetProcessList(runningProcesses);
	for (ExecutableSwitch &s : executableSwitches) {
		if (!s.initialized())
			continue;
		// True if executable switch is running (direct)
		bool equals = runningProcesses.contains(s.exe);
		// True if executable switch is running (regex)
		bool matches = (runningProcesses.indexOf(
					QRegularExpression(s.exe)) != -1);
		// True if focus is disabled OR switch is focused
		bool focus = (!s.inFocus || isInFocus(s.exe));
		// True if current window is ignored AND switch equals OR matches last window
		bool ignore =
			(ignored && (title == s.exe.toStdString() ||
				     QString::fromStdString(title).contains(
					     QRegularExpression(s.exe))));

		if ((equals || matches) && (focus || ignore)) {
			match = true;
			scene = s.scene;
			transition = s.transition;

			if (verbose)
				s.logMatch();
			break;
		}
	}
}

void SwitcherData::saveExecutableSwitches(obs_data_t *obj)
{
	obs_data_array_t *executableArray = obs_data_array_create();
	for (ExecutableSwitch &s : switcher->executableSwitches) {
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
			obs_data_set_string(array_obj, "exefile",
					    s.exe.toUtf8());
			obs_data_set_bool(array_obj, "infocus", s.inFocus);
			obs_data_array_push_back(executableArray, array_obj);
		}
		obs_source_release(source);
		obs_source_release(transition);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "executableSwitches", executableArray);
	obs_data_array_release(executableArray);
}

void SwitcherData::loadExecutableSwitches(obs_data_t *obj)
{
	switcher->executableSwitches.clear();

	obs_data_array_t *executableArray =
		obs_data_get_array(obj, "executableSwitches");
	size_t count = obs_data_array_count(executableArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(executableArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		const char *exe = obs_data_get_string(array_obj, "exefile");
		bool infocus = obs_data_get_bool(array_obj, "infocus");

		switcher->executableSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), exe, infocus);

		obs_data_release(array_obj);
	}
	obs_data_array_release(executableArray);
}

void AdvSceneSwitcher::setupExecutableTab()
{
	for (auto &s : switcher->executableSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->executables);
		ui->executables->addItem(item);
		ExecutableSwitchWidget *sw = new ExecutableSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->executables->setItemWidget(item, sw);
	}

	if (switcher->executableSwitches.size() == 0)
		addPulse = PulseWidget(ui->executableAdd, QColor(Qt::green));
}

ExecutableSwitchWidget::ExecutableSwitchWidget(ExecutableSwitch *s)
	: SwitchWidget(s, false)
{
	processes = new QComboBox();
	requiresFocus = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.executableTab.requiresFocus"));

	QWidget::connect(processes, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(ProcessChanged(const QString &)));
	QWidget::connect(requiresFocus, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusChanged(int)));

	AdvSceneSwitcher::populateProcessSelection(processes);

	processes->setEditable(true);
	processes->setMaxVisibleItems(20);

	if (s) {
		processes->setCurrentText(s->exe);
		requiresFocus->setChecked(s->inFocus);
	}

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{processes}}", processes},
		{"{{requiresFocus}}", requiresFocus},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.executableTab.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

ExecutableSwitch *ExecutableSwitchWidget::getSwitchData()
{
	return switchData;
}

void ExecutableSwitchWidget::setSwitchData(ExecutableSwitch *s)
{
	switchData = s;
}

void ExecutableSwitchWidget::swapSwitchData(ExecutableSwitchWidget *s1,
					    ExecutableSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	ExecutableSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void ExecutableSwitchWidget::ProcessChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->exe = text;
}

void ExecutableSwitchWidget::FocusChanged(int state)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->inFocus = state;
}
