#include <regex>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_pauseAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->pauseEntries.emplace_back();

	listAddClicked(ui->pauseEntries,
		       new PauseEntryWidget(&switcher->pauseEntries.back()),
		       ui->pauseAdd, &addPulse);
}

void AdvSceneSwitcher::on_pauseRemove_clicked()
{
	QListWidgetItem *item = ui->pauseEntries->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->pauseEntries->currentRow();
		auto &switches = switcher->pauseEntries;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_pauseUp_clicked()
{
	int index = ui->pauseEntries->currentRow();
	if (!listMoveUp(ui->pauseEntries))
		return;

	PauseEntryWidget *s1 = (PauseEntryWidget *)ui->pauseEntries->itemWidget(
		ui->pauseEntries->item(index));
	PauseEntryWidget *s2 = (PauseEntryWidget *)ui->pauseEntries->itemWidget(
		ui->pauseEntries->item(index - 1));
	PauseEntryWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->pauseEntries[index],
		  switcher->pauseEntries[index - 1]);
}

void AdvSceneSwitcher::on_pauseDown_clicked()
{
	int index = ui->pauseEntries->currentRow();

	if (!listMoveDown(ui->pauseEntries))
		return;

	PauseEntryWidget *s1 = (PauseEntryWidget *)ui->pauseEntries->itemWidget(
		ui->pauseEntries->item(index));
	PauseEntryWidget *s2 = (PauseEntryWidget *)ui->pauseEntries->itemWidget(
		ui->pauseEntries->item(index + 1));
	PauseEntryWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->pauseEntries[index],
		  switcher->pauseEntries[index + 1]);
}

void AdvSceneSwitcher::on_pauseWindowsAdd_clicked()
{
	QString windowName = ui->pauseWindowsWindows->currentText();

	if (windowName.isEmpty())
		return;

	QVariant v = QVariant::fromValue(windowName);

	QList<QListWidgetItem *> items =
		ui->pauseWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item =
			new QListWidgetItem(windowName, ui->pauseWindows);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->pauseWindowsSwitches.emplace_back(
			windowName.toUtf8().constData());
		ui->pauseWindows->sortItems();
	}
}

void AdvSceneSwitcher::on_pauseWindowsRemove_clicked()
{
	QListWidgetItem *item = ui->pauseWindows->currentItem();
	if (!item)
		return;

	QString windowName = item->data(Qt::UserRole).toString();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->pauseWindowsSwitches;

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

void AdvSceneSwitcher::on_pauseWindows_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->pauseWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->pauseWindowsSwitches) {
		if (window.compare(s.c_str()) == 0) {
			ui->pauseWindowsWindows->setCurrentText(s.c_str());
			break;
		}
	}
}

int AdvSceneSwitcher::PauseWindowsFindByData(const QString &window)
{
	int count = ui->pauseWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->pauseWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

bool SwitcherData::checkPause()
{
	bool pause = false;
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	for (OBSWeakSource &s : pauseScenesSwitches) {
		if (s == ws) {
			pause = true;
			break;
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);

	std::string title;
	if (!pause) {
		GetCurrentWindowTitle(title);
		for (std::string &window : pauseWindowsSwitches) {
			if (window == title) {
				pause = true;
				break;
			}
		}
	}

	if (!pause) {
		GetCurrentWindowTitle(title);
		for (std::string &window : pauseWindowsSwitches) {
			try {
				bool matches = std::regex_match(
					title, std::regex(window));
				if (matches) {
					pause = true;
					break;
				}
			} catch (const std::regex_error &) {
			}
		}
	}

	if (verbose && pause)
		blog(LOG_INFO, "pause match");

	return pause;
}

void SwitcherData::savePauseSwitches(obs_data_t *obj)
{
	obs_data_array_t *pauseScenesArray = obs_data_array_create();
	for (OBSWeakSource &scene : switcher->pauseScenesSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(scene);
		if (source) {
			const char *n = obs_source_get_name(source);
			obs_data_set_string(array_obj, "pauseScene", n);
			obs_data_array_push_back(pauseScenesArray, array_obj);
		}
		obs_source_release(source);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "pauseEntries", pauseScenesArray);
	obs_data_array_release(pauseScenesArray);

	obs_data_array_t *pauseWindowsArray = obs_data_array_create();
	for (std::string &window : switcher->pauseWindowsSwitches) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_string(array_obj, "pauseWindow", window.c_str());
		obs_data_array_push_back(pauseWindowsArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "pauseWindows", pauseWindowsArray);
	obs_data_array_release(pauseWindowsArray);
}

void SwitcherData::loadPauseSwitches(obs_data_t *obj)
{
	switcher->pauseScenesSwitches.clear();

	obs_data_array_t *pauseScenesArray =
		obs_data_get_array(obj, "pauseEntries");
	size_t count = obs_data_array_count(pauseScenesArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(pauseScenesArray, i);

		const char *scene =
			obs_data_get_string(array_obj, "pauseScene");

		switcher->pauseScenesSwitches.emplace_back(
			GetWeakSourceByName(scene));

		obs_data_release(array_obj);
	}
	obs_data_array_release(pauseScenesArray);

	switcher->pauseWindowsSwitches.clear();

	obs_data_array_t *pauseWindowsArray =
		obs_data_get_array(obj, "pauseWindows");
	count = obs_data_array_count(pauseWindowsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(pauseWindowsArray, i);

		const char *window =
			obs_data_get_string(array_obj, "pauseWindow");

		switcher->pauseWindowsSwitches.emplace_back(window);

		obs_data_release(array_obj);
	}
	obs_data_array_release(pauseWindowsArray);
}

void AdvSceneSwitcher::setupPauseTab()
{
	populateSceneSelection(ui->pauseScenesScenes, false);
	populateWindowSelection(ui->pauseWindowsWindows);

	for (auto &scene : switcher->pauseScenesSwitches) {
		std::string sceneName = GetWeakSourceName(scene);
		QString text = QString::fromStdString(sceneName);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->pauseEntries);
		item->setData(Qt::UserRole, text);
	}

	for (auto &window : switcher->pauseWindowsSwitches) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->pauseWindows);
		item->setData(Qt::UserRole, text);
	}
}

void populatePauseTypes(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.pauseTab.pauseTypeScene"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.pauseTab.pauseTypeWindow"));
}

void populatePauseTargets(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.pauseTab.pauseTargetAll"));
	list->addItem(obs_module_text("AdvSceneSwitcher.windowTitleTab.title"));
	list->addItem(obs_module_text("AdvSceneSwitcher.executableTab.title"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.screenRegionTab.title"));
	list->addItem(obs_module_text("AdvSceneSwitcher.mediaTab.title"));
	list->addItem(obs_module_text("AdvSceneSwitcher.fileTab.title"));
	list->addItem(obs_module_text("AdvSceneSwitcher.randomTab.title"));
	list->addItem(obs_module_text("AdvSceneSwitcher.timeTab.title"));
	list->addItem(obs_module_text("AdvSceneSwitcher.idleTab.title"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.sceneSequenceTab.title"));
	list->addItem(obs_module_text("AdvSceneSwitcher.audioTab.title"));
}

PauseEntryWidget::PauseEntryWidget(PauseEntry *s) : SwitchWidget(s, false)
{
	pauseTypes = new QComboBox();
	pauseTargets = new QComboBox();
	windows = new QComboBox();

	QWidget::connect(pauseTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(PauseTypeChanged(int)));
	QWidget::connect(pauseTargets, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(PauseTargetChanged(int)));
	QWidget::connect(windows, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(WindowChanged(const QString &)));

	populatePauseTypes(pauseTypes);
	populatePauseTargets(pauseTargets);
	AdvSceneSwitcher::populateWindowSelection(windows);

	if (s) {
		scenes->setCurrentText(GetWeakSourceName(s->scene).c_str());
		pauseTypes->setCurrentIndex(static_cast<int>(s->pauseType));
		pauseTargets->setCurrentIndex(static_cast<int>(s->pauseTarget));
		windows->setCurrentText(s->window.c_str());
		if (s->pauseType == PauseType::Scene) {
			windows->setDisabled(true);
			windows->setVisible(false);
		} else {
			scenes->setDisabled(true);
			scenes->setVisible(false);
		}
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", scenes},
		{"{{pauseTypes}}", pauseTypes},
		{"{{pauseTargets}}", pauseTargets},
		{"{{windows}}", windows}};

	placeWidgets(obs_module_text("AdvSceneSwitcher.pauseTab.pauseEntry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

PauseEntry *PauseEntryWidget::getSwitchData()
{
	return switchData;
}

void PauseEntryWidget::setSwitchData(PauseEntry *s)
{
	switchData = s;
}

void PauseEntryWidget::swapSwitchData(PauseEntryWidget *s1,
				      PauseEntryWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	PauseEntry *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void PauseEntryWidget::PauseTypeChanged(int index)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->pauseType = static_cast<PauseType>(index);

	if (switchData->pauseType == PauseType::Scene) {
		windows->setDisabled(true);
		windows->setVisible(false);
		scenes->setDisabled(false);
		scenes->setVisible(true);
	} else {
		scenes->setDisabled(true);
		scenes->setVisible(false);
		windows->setDisabled(false);
		windows->setVisible(true);
	}
}

void PauseEntryWidget::PauseTargetChanged(int index)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->pauseTarget = static_cast<PauseTarget>(index);
}

void PauseEntryWidget::WindowChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->window = text.toStdString();
}
