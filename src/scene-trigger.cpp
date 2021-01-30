#include <thread>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool SceneTrigger::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_triggerAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->sceneTriggers.emplace_back();

	listAddClicked(ui->sceneTriggers,
		       new SceneTriggerWidget(this,
					      &switcher->sceneTriggers.back()),
		       ui->triggerAdd, &addPulse);

	ui->triggerHelp->setVisible(false);
}

void AdvSceneSwitcher::on_triggerRemove_clicked()
{
	QListWidgetItem *item = ui->sceneTriggers->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->sceneTriggers->currentRow();
		auto &switches = switcher->sceneTriggers;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_triggerUp_clicked()
{
	int index = ui->sceneTriggers->currentRow();
	if (!listMoveUp(ui->sceneTriggers))
		return;

	SceneTriggerWidget *s1 =
		(SceneTriggerWidget *)ui->sceneTriggers->itemWidget(
			ui->sceneTriggers->item(index));
	SceneTriggerWidget *s2 =
		(SceneTriggerWidget *)ui->sceneTriggers->itemWidget(
			ui->sceneTriggers->item(index - 1));
	SceneTriggerWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneTriggers[index],
		  switcher->sceneTriggers[index - 1]);
}

void AdvSceneSwitcher::on_triggerDown_clicked()
{
	int index = ui->sceneTriggers->currentRow();

	if (!listMoveDown(ui->sceneTriggers))
		return;

	SceneTriggerWidget *s1 =
		(SceneTriggerWidget *)ui->sceneTriggers->itemWidget(
			ui->sceneTriggers->item(index));
	SceneTriggerWidget *s2 =
		(SceneTriggerWidget *)ui->sceneTriggers->itemWidget(
			ui->sceneTriggers->item(index + 1));
	SceneTriggerWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneTriggers[index],
		  switcher->sceneTriggers[index + 1]);
}

void SceneTrigger::logMatch()
{
	const char *actionName = "";

	switch (triggerAction) {
	case sceneTriggerAction::NONE:
		actionName = "NONE";
		break;
	case sceneTriggerAction::START_RECORDING:
		actionName = "START RECORDING";
		break;
	case sceneTriggerAction::PAUSE_RECORDING:
		actionName = "PAUSE RECORDING";
		break;
	case sceneTriggerAction::UNPAUSE_RECORDING:
		actionName = "UNPAUSE RECORDING";
		break;
	case sceneTriggerAction::STOP_RECORDING:
		actionName = "STOP RECORDING";
		break;
	case sceneTriggerAction::START_STREAMING:
		actionName = "START STREAMING";
		break;
	case sceneTriggerAction::STOP_STREAMING:
		actionName = "STOP STREAMING";
		break;
	case sceneTriggerAction::START_REPLAY_BUFFER:
		actionName = "START REPLAY BUFFER";
		break;
	case sceneTriggerAction::STOP_REPLAY_BUFFER:
		actionName = "STOP REPLAY BUFFER";
		break;
	default:
		break;
	}

	blog(LOG_INFO, "triggering action '%s' after %f seconds", actionName,
	     duration);
}

void actionThread(sceneTriggerAction action, double delay)
{
	long long mil = delay * 1000;
	std::this_thread::sleep_for(std::chrono::milliseconds(mil));

	switch (action) {
	case sceneTriggerAction::NONE:
		break;
	case sceneTriggerAction::START_RECORDING:
		obs_frontend_recording_start();
		break;
	case sceneTriggerAction::PAUSE_RECORDING:
		obs_frontend_recording_pause(true);
		break;
	case sceneTriggerAction::UNPAUSE_RECORDING:
		obs_frontend_recording_pause(false);
		break;
	case sceneTriggerAction::STOP_RECORDING:
		obs_frontend_recording_stop();
		break;
	case sceneTriggerAction::START_STREAMING:
		obs_frontend_streaming_start();
		break;
	case sceneTriggerAction::STOP_STREAMING:
		obs_frontend_streaming_stop();
		break;
	case sceneTriggerAction::START_REPLAY_BUFFER:
		obs_frontend_replay_buffer_start();
		break;
	case sceneTriggerAction::STOP_REPLAY_BUFFER:
		obs_frontend_replay_buffer_stop();
		break;
	default:
		break;
	}
}

void SceneTrigger::performAction()
{
	if (triggerAction == sceneTriggerAction::NONE)
		return;

	std::thread t(actionThread, triggerAction, duration);
	t.detach();
}

bool SceneTrigger::checkMatch(OBSWeakSource previousScene)
{
	OBSSource source = obs_frontend_get_current_scene();
	OBSWeakSource currentScene = obs_source_get_weak_source(source);
	obs_source_release(source);
	obs_weak_source_release(currentScene);

	switch (triggerType) {
	case sceneTriggerType::NONE:
		return false;
	case sceneTriggerType::SCENE_ACTIVE:
		return currentScene == scene;
	case sceneTriggerType::SCENE_INACTIVE:
		return currentScene != scene;
	case sceneTriggerType::SCENE_LEAVE:
		return previousScene == scene;
	}
	return false;
}

void SwitcherData::checkTriggers()
{
	if (SceneTrigger::pause) {
		return;
	}

	for (auto &t : sceneTriggers) {
		if (t.checkMatch(previousScene)) {
			if (verbose) {
				t.logMatch();
			}

			t.performAction();
		}
	}
}

void SwitcherData::saveSceneTriggers(obs_data_t *obj)
{
	obs_data_array_t *triggerArray = obs_data_array_create();
	for (auto &s : switcher->sceneTriggers) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(triggerArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "triggers", triggerArray);
	obs_data_array_release(triggerArray);
}

void SwitcherData::loadSceneTriggers(obs_data_t *obj)
{
	switcher->sceneTriggers.clear();

	obs_data_array_t *triggerArray = obs_data_get_array(obj, "triggers");
	size_t count = obs_data_array_count(triggerArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(triggerArray, i);

		switcher->sceneTriggers.emplace_back();
		sceneTriggers.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(triggerArray);
}

void AdvSceneSwitcher::setupTriggerTab()
{
	for (auto &s : switcher->sceneTriggers) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->sceneTriggers);
		ui->sceneTriggers->addItem(item);
		SceneTriggerWidget *sw = new SceneTriggerWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->sceneTriggers->setItemWidget(item, sw);
	}

	if (switcher->sceneTriggers.size() == 0) {
		addPulse = PulseWidget(ui->triggerAdd, QColor(Qt::green));
		ui->triggerHelp->setVisible(true);
	} else {
		ui->triggerHelp->setVisible(false);
	}
}

void SceneTrigger::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj, "unused", "scene", "unused");

	obs_data_set_int(obj, "triggerType", static_cast<int>(triggerType));
	obs_data_set_int(obj, "triggerAction", static_cast<int>(triggerAction));
}

void SceneTrigger::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj, "unused", "scene", "unused2");

	triggerType = static_cast<sceneTriggerType>(
		obs_data_get_int(obj, "triggerType"));
	triggerAction = static_cast<sceneTriggerAction>(
		obs_data_get_int(obj, "triggerAction"));
}

inline void populateTriggers(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerType.none"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerType.sceneActive"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerType.sceneInactive"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerType.sceneLeave"));
}

inline void populateActions(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.none"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.startRecording"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.pauseRecording"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.unpauseRecording"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.stopRecording"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.startStreaming"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.stopStreaming"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.startReplayBuffer"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.stopReplayBuffer"));
}

SceneTriggerWidget::SceneTriggerWidget(QWidget *parent, SceneTrigger *s)
	: SwitchWidget(parent, s, false, false)
{
	triggers = new QComboBox();
	actions = new QComboBox();
	duration = new QDoubleSpinBox();

	duration->setMinimum(0.0);
	duration->setMaximum(99.000000);
	duration->setSuffix("s");

	QWidget::connect(triggers, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TriggerTypeChanged(int)));
	QWidget::connect(actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TriggerActionChanged(int)));
	QWidget::connect(duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));

	populateTriggers(triggers);
	populateActions(actions);

	if (s) {
		triggers->setCurrentIndex(static_cast<int>(s->triggerType));
		actions->setCurrentIndex(static_cast<int>(s->triggerAction));
		duration->setValue(s->duration);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{triggers}}", triggers},
		{"{{actions}}", actions},
		{"{{duration}}", duration},
		{"{{scenes}}", scenes}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.sceneTriggerTab.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

SceneTrigger *SceneTriggerWidget::getSwitchData()
{
	return switchData;
}

void SceneTriggerWidget::setSwitchData(SceneTrigger *s)
{
	switchData = s;
}

void SceneTriggerWidget::swapSwitchData(SceneTriggerWidget *s1,
					SceneTriggerWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	SceneTrigger *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void SceneTriggerWidget::TriggerTypeChanged(int index)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->triggerType = static_cast<sceneTriggerType>(index);
}

void SceneTriggerWidget::TriggerActionChanged(int index)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->triggerAction = static_cast<sceneTriggerAction>(index);
}

void SceneTriggerWidget::DurationChanged(double dur)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}
