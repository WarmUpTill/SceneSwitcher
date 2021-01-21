#include <regex>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_pauseAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->pauseEntries.emplace_back();

	listAddClicked(ui->pauseEntries,
		       new PauseEntryWidget(this, &switcher->pauseEntries.back()),
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

void resetPause()
{
	DefaultSceneTransition::pause = false;
	WindowSwitch::pause = false;
	ExecutableSwitch::pause = false;
	ScreenRegionSwitch::pause = false;
	MediaSwitch::pause = false;
	FileSwitch::pause = false;
	RandomSwitch::pause = false;
	TimeSwitch::pause = false;
	IdleData::pause = false;
	SceneSequenceSwitch::pause = false;
	AudioSwitch::pause = false;
}

void setPauseTarget(PauseTarget &target, bool &verbose)
{
	switch (target) {
	case PauseTarget::All:
		if (verbose)
			blog(LOG_INFO, "pause all switching");
		break;
	case PauseTarget::Transition:
		if (verbose)
			blog(LOG_INFO, "pause def_transition switching");
		DefaultSceneTransition::pause = true;
		break;
	case PauseTarget::Window:
		if (verbose)
			blog(LOG_INFO, "pause window switching");
		WindowSwitch::pause = true;
		break;
	case PauseTarget::Executable:
		if (verbose)
			blog(LOG_INFO, "pause exec switching");
		ExecutableSwitch::pause = true;
		break;
	case PauseTarget::Region:
		if (verbose)
			blog(LOG_INFO, "pause region switching");
		ScreenRegionSwitch::pause = true;
		break;
	case PauseTarget::Media:
		if (verbose)
			blog(LOG_INFO, "pause media switching");
		MediaSwitch::pause = true;
		break;
	case PauseTarget::File:
		if (verbose)
			blog(LOG_INFO, "pause file switching");
		FileSwitch::pause = true;
		break;
	case PauseTarget::Random:
		if (verbose)
			blog(LOG_INFO, "pause random switching");
		RandomSwitch::pause = true;
		break;
	case PauseTarget::Time:
		if (verbose)
			blog(LOG_INFO, "pause time switching");
		TimeSwitch::pause = true;
		break;
	case PauseTarget::Idle:
		if (verbose)
			blog(LOG_INFO, "pause idle switching");
		IdleData::pause = true;
		break;
	case PauseTarget::Sequence:
		if (verbose)
			blog(LOG_INFO, "pause sequence switching");
		SceneSequenceSwitch::pause = true;
		break;
	case PauseTarget::Audio:
		if (verbose)
			blog(LOG_INFO, "pause audio switching");
		AudioSwitch::pause = true;
		break;
	}
}

bool checkPauseScene(obs_weak_source_t *currentScene, obs_weak_source_t *scene,
		     PauseTarget &target, bool &verbose)
{
	if (currentScene != scene)
		return false;

	setPauseTarget(target, verbose);
	if (target == PauseTarget::All)
		return true;
	return false;
}
bool checkPauseWindow(std::string &currentTitle, std::string &title,
		      PauseTarget &target, bool &verbose)
{
	if (currentTitle != title)
		return false;

	setPauseTarget(target, verbose);
	if (target == PauseTarget::All)
		return true;
	return false;
}

bool SwitcherData::checkPause()
{
	bool pauseAll = false;

	std::string title;
	GetCurrentWindowTitle(title);

	resetPause();

	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	for (PauseEntry &s : pauseEntries) {
		if (s.pauseType == PauseType::Scene)
			pauseAll = checkPauseScene(ws, s.scene, s.pauseTarget,
						   verbose);
		else
			pauseAll = checkPauseWindow(title, s.window,
						    s.pauseTarget, verbose);
		if (pauseAll)
			break;
	}

	obs_source_release(currentSource);
	obs_weak_source_release(ws);

	return pauseAll;
}

void SwitcherData::savePauseSwitches(obs_data_t *obj)
{
	obs_data_array_t *pauseScenesArray = obs_data_array_create();
	for (PauseEntry &s : switcher->pauseEntries) {
		obs_data_t *array_obj = obs_data_create();

		obs_data_set_int(array_obj, "pauseType",
				 static_cast<int>(s.pauseType));
		obs_data_set_int(array_obj, "pauseTarget",
				 static_cast<int>(s.pauseTarget));
		obs_data_set_string(array_obj, "pauseWindow", s.window.c_str());

		obs_source_t *source = obs_weak_source_get_source(s.scene);
		if (source) {
			const char *n = obs_source_get_name(source);
			obs_data_set_string(array_obj, "pauseScene", n);
		}

		obs_data_array_push_back(pauseScenesArray, array_obj);

		obs_source_release(source);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "pauseEntries", pauseScenesArray);
	obs_data_set_int(obj, "oldPauseValuesImported", 1);
	obs_data_array_release(pauseScenesArray);
}

void SwitcherData::loadPauseSwitches(obs_data_t *obj)
{
	switcher->pauseEntries.clear();

	// To be removed in future builds once most users have migrated
	loadOldPauseSwitches(obj);

	obs_data_array_t *pauseArray = obs_data_get_array(obj, "pauseEntries");
	size_t count = obs_data_array_count(pauseArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(pauseArray, i);

		PauseType type = static_cast<PauseType>(
			obs_data_get_int(array_obj, "pauseType"));
		PauseTarget target = static_cast<PauseTarget>(
			obs_data_get_int(array_obj, "pauseTarget"));
		const char *scene =
			obs_data_get_string(array_obj, "pauseScene");
		const char *window =
			obs_data_get_string(array_obj, "pauseWindow");

		switcher->pauseEntries.emplace_back(GetWeakSourceByName(scene),
						    type, target, window);

		obs_data_release(array_obj);
	}
	obs_data_array_release(pauseArray);
}

void SwitcherData::loadOldPauseSwitches(obs_data_t *obj)
{
	if (obs_data_get_int(obj, "oldPauseValuesImported"))
		return;

	obs_data_array_t *pauseScenesArray =
		obs_data_get_array(obj, "pauseScenes");
	size_t count = obs_data_array_count(pauseScenesArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(pauseScenesArray, i);

		const char *scene =
			obs_data_get_string(array_obj, "pauseScene");

		switcher->pauseEntries.emplace_back(GetWeakSourceByName(scene),
						    PauseType::Scene,
						    PauseTarget::All, "");

		obs_data_release(array_obj);
	}
	obs_data_array_release(pauseScenesArray);

	obs_data_array_t *pauseWindowsArray =
		obs_data_get_array(obj, "pauseWindows");
	count = obs_data_array_count(pauseWindowsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(pauseWindowsArray, i);

		const char *window =
			obs_data_get_string(array_obj, "pauseWindow");

		switcher->pauseEntries.emplace_back(nullptr, PauseType::Window,
						    PauseTarget::All, window);

		obs_data_release(array_obj);
	}
	obs_data_array_release(pauseWindowsArray);
}

void AdvSceneSwitcher::setupPauseTab()
{
	for (auto &s : switcher->pauseEntries) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->pauseEntries);
		ui->pauseEntries->addItem(item);
		PauseEntryWidget *sw = new PauseEntryWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->pauseEntries->setItemWidget(item, sw);
	}

	if (switcher->pauseEntries.size() == 0)
		addPulse = PulseWidget(ui->pauseAdd, QColor(Qt::green));
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
	list->addItem(obs_module_text("AdvSceneSwitcher.transitionTab.title"));
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

PauseEntryWidget::PauseEntryWidget(QWidget *parent, PauseEntry *s)
	: SwitchWidget(parent, s, false, false)
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

	windows->setEditable(true);
	windows->setMaxVisibleItems(20);

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

bool PauseEntry::valid()
{
	return true;
}
