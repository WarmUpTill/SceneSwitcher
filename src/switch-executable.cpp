#include "headers/advanced-scene-switcher.hpp"

int SceneSwitcher::executableFindByData(const QString &exe)
{
	int count = ui->executables->count();

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->executables->item(i);
		QString itemExe = item->data(Qt::UserRole).toString();

		if (itemExe == exe)
			return i;
	}

	return -1;
}

void SceneSwitcher::on_executables_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->executables->item(idx);

	QString exec = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->executableSwitches) {
		if (exec.compare(s.exe) == 0) {
			QString sceneName = GetWeakSourceName(s.scene).c_str();
			QString transitionName =
				GetWeakSourceName(s.transition).c_str();
			ui->executableScenes->setCurrentText(sceneName);
			ui->executable->setCurrentText(exec);
			ui->executableTransitions->setCurrentText(
				transitionName);
			ui->requiresFocusCheckBox->setChecked(s.inFocus);
			break;
		}
	}
}

void SceneSwitcher::on_executableUp_clicked()
{
	int index = ui->executables->currentRow();
	if (index != -1 && index != 0) {
		ui->executables->insertItem(index - 1,
					    ui->executables->takeItem(index));
		ui->executables->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->executableSwitches.begin() + index,
			  switcher->executableSwitches.begin() + index - 1);
	}
}

void SceneSwitcher::on_executableDown_clicked()
{
	int index = ui->executables->currentRow();
	if (index != -1 && index != ui->executables->count() - 1) {
		ui->executables->insertItem(index + 1,
					    ui->executables->takeItem(index));
		ui->executables->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->executableSwitches.begin() + index,
			  switcher->executableSwitches.begin() + index + 1);
	}
}

void SceneSwitcher::on_executableAdd_clicked()
{
	QString sceneName = ui->executableScenes->currentText();
	QString exeName = ui->executable->currentText();
	QString transitionName = ui->executableTransitions->currentText();
	bool inFocus = ui->requiresFocusCheckBox->isChecked();

	if (exeName.isEmpty() || sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);
	QVariant v = QVariant::fromValue(exeName);

	QString text = MakeSwitchNameExecutable(sceneName, exeName,
						transitionName, inFocus);

	int idx = executableFindByData(exeName);

	if (idx == -1) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->executableSwitches.emplace_back(
			source, transition, exeName.toUtf8().constData(),
			inFocus);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->executables);
		item->setData(Qt::UserRole, v);
	} else {
		QListWidgetItem *item = ui->executables->item(idx);
		item->setText(text);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->executableSwitches) {
				if (s.exe == exeName) {
					s.scene = source;
					s.transition = transition;
					s.inFocus = inFocus;
					break;
				}
			}
		}
	}
}

void SceneSwitcher::on_executableRemove_clicked()
{
	QListWidgetItem *item = ui->executables->currentItem();
	if (!item)
		return;

	QString exe = item->data(Qt::UserRole).toString();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->executableSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.exe == exe) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
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
	for (ExecutableSceneSwitch &s : executableSwitches) {
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
	for (ExecutableSceneSwitch &s : switcher->executableSwitches) {
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
			obs_source_release(source);
			obs_source_release(transition);
		}

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

void SceneSwitcher::setupExecutableTab()
{
	populateSceneSelection(ui->executableScenes, false);
	populateTransitionSelection(ui->executableTransitions);

	QStringList processes;
	GetProcessList(processes);
	for (QString &process : processes)
		ui->executable->addItem(process);

	for (auto &s : switcher->executableSwitches) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSwitchNameExecutable(sceneName.c_str(),
							s.exe,
							transitionName.c_str(),
							s.inFocus);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->executables);
		item->setData(Qt::UserRole, s.exe);
	}
}

static inline QString MakeSwitchNameExecutable(const QString &scene,
					       const QString &value,
					       const QString &transition,
					       bool inFocus)
{
	if (!inFocus)
		return QStringLiteral("[") + scene + QStringLiteral(", ") +
		       transition + QStringLiteral("]: ") + value;
	return QStringLiteral("[") + scene + QStringLiteral(", ") + transition +
	       QStringLiteral("]: ") + value +
	       QStringLiteral(" (only if focused)");
}
