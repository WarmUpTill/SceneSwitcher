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
	if (!item) {
		return;
	}

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
	if (!listMoveUp(ui->sceneTriggers)) {
		return;
	}

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

	if (!listMoveDown(ui->sceneTriggers)) {
		return;
	}

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
	std::string sceneName = "";
	std::string statusName = "";
	std::string actionName = "";

	switch (triggerType) {
	case sceneTriggerType::NONE:
		statusName = "NONE";
		break;
	case sceneTriggerType::SCENE_ACTIVE:
		statusName = "SCENE ACTIVE";
		break;
	case sceneTriggerType::SCENE_INACTIVE:
		statusName = "SCENE INACTIVE";
		break;
	case sceneTriggerType::SCENE_LEAVE:
		statusName = "SCENE LEAVE";
		break;
	default:
		break;
	}

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
	case sceneTriggerAction::MUTE_SOURCE:
		actionName = "MUTE (" + GetWeakSourceName(audioSource) + ")";
		break;
	default:
		actionName = "UNKOWN";
		break;
	}

	blog(LOG_INFO,
	     "scene '%s' in status '%s' triggering action '%s' after %f seconds",
	     GetWeakSourceName(scene).c_str(), statusName.c_str(),
	     actionName.c_str(), duration);
}

void frontEndActionThread(sceneTriggerAction action, double delay)
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
	case sceneTriggerAction::MUTE_SOURCE:
		obs_frontend_replay_buffer_stop();
		break;
	default:
		break;
	}
}

void muteThread(OBSWeakSource source, double delay)
{
	long long mil = delay * 1000;
	std::this_thread::sleep_for(std::chrono::milliseconds(mil));

	auto s = obs_weak_source_get_source(source);
	obs_source_set_muted(s, true);
	obs_source_release(s);
}

bool isFrontendAction(sceneTriggerAction triggerAction)
{
	return triggerAction == sceneTriggerAction::START_RECORDING ||
	       triggerAction == sceneTriggerAction::PAUSE_RECORDING ||
	       triggerAction == sceneTriggerAction::UNPAUSE_RECORDING ||
	       triggerAction == sceneTriggerAction::STOP_RECORDING ||
	       triggerAction == sceneTriggerAction::START_STREAMING ||
	       triggerAction == sceneTriggerAction::STOP_STREAMING ||
	       triggerAction == sceneTriggerAction::START_REPLAY_BUFFER ||
	       triggerAction == sceneTriggerAction::STOP_REPLAY_BUFFER;
}

void SceneTrigger::performAction()
{
	if (triggerAction == sceneTriggerAction::NONE) {
		return;
	}

	std::thread t;

	if (isFrontendAction(triggerAction)) {
		t = std::thread(frontEndActionThread, triggerAction, duration);
	} else {
		t = std::thread(muteThread, audioSource, duration);
	}

	t.detach();
}

bool SceneTrigger::checkMatch(OBSWeakSource currentScene,
			      OBSWeakSource previousScene)
{
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
	if (SceneTrigger::pause || stop) {
		return;
	}

	OBSSource source = obs_frontend_get_current_scene();
	OBSWeakSource currentScene = obs_source_get_weak_source(source);

	for (auto &t : sceneTriggers) {
		if (t.checkMatch(currentScene, previousScene)) {
			t.logMatch();
			t.performAction();
		}
	}

	obs_source_release(source);
	obs_weak_source_release(currentScene);
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

// To be removed in future version
void loadOldAutoStopStart(obs_data_t *obj)
{
	typedef enum {
		RECORDING = 0,
		STREAMING = 1,
		RECORINDGSTREAMING = 2
	} AutoStartStopType;

	if (obs_data_get_bool(obj, "autoStopEnable")) {
		std::string autoStopScene =
			obs_data_get_string(obj, "autoStopSceneName");
		int action = obs_data_get_int(obj, "autoStopType");

		if (action == RECORDING) {
			switcher->sceneTriggers.emplace_back();
			auto &s = switcher->sceneTriggers.back();
			s.scene = GetWeakSourceByName(autoStopScene.c_str());
			s.triggerType = sceneTriggerType::SCENE_ACTIVE;
			s.triggerAction = sceneTriggerAction::STOP_RECORDING;
		}

		if (action == STREAMING) {
			switcher->sceneTriggers.emplace_back();
			auto &s = switcher->sceneTriggers.back();
			s.scene = GetWeakSourceByName(autoStopScene.c_str());
			s.triggerType = sceneTriggerType::SCENE_ACTIVE;
			s.triggerAction = sceneTriggerAction::STOP_STREAMING;
		}

		if (action == RECORINDGSTREAMING) {
			switcher->sceneTriggers.emplace_back();
			auto &s = switcher->sceneTriggers.back();
			s.scene = GetWeakSourceByName(autoStopScene.c_str());
			s.triggerType = sceneTriggerType::SCENE_ACTIVE;
			s.triggerAction = sceneTriggerAction::STOP_RECORDING;

			switcher->sceneTriggers.emplace_back();
			auto &s2 = switcher->sceneTriggers.back();
			s2.scene = GetWeakSourceByName(autoStopScene.c_str());
			s2.triggerType = sceneTriggerType::SCENE_ACTIVE;
			s2.triggerAction = sceneTriggerAction::STOP_STREAMING;
		}
	}

	if (obs_data_get_bool(obj, "autoStartEnable")) {
		std::string autoStartScene =
			obs_data_get_string(obj, "autoStartSceneName");
		int action = obs_data_get_int(obj, "autoStartType");

		if (action == RECORDING) {
			switcher->sceneTriggers.emplace_back();
			auto &s = switcher->sceneTriggers.back();
			s.scene = GetWeakSourceByName(autoStartScene.c_str());
			s.triggerType = sceneTriggerType::SCENE_ACTIVE;
			s.triggerAction = sceneTriggerAction::START_RECORDING;
		}

		if (action == STREAMING) {
			switcher->sceneTriggers.emplace_back();
			auto &s = switcher->sceneTriggers.back();
			s.scene = GetWeakSourceByName(autoStartScene.c_str());
			s.triggerType = sceneTriggerType::SCENE_ACTIVE;
			s.triggerAction = sceneTriggerAction::START_STREAMING;
		}

		if (action == RECORINDGSTREAMING) {
			switcher->sceneTriggers.emplace_back();
			auto &s = switcher->sceneTriggers.back();
			s.scene = GetWeakSourceByName(autoStartScene.c_str());
			s.triggerType = sceneTriggerType::SCENE_ACTIVE;
			s.triggerAction = sceneTriggerAction::START_RECORDING;

			switcher->sceneTriggers.emplace_back();
			auto &s2 = switcher->sceneTriggers.back();
			s2.scene = GetWeakSourceByName(autoStartScene.c_str());
			s2.triggerType = sceneTriggerType::SCENE_ACTIVE;
			s2.triggerAction = sceneTriggerAction::START_STREAMING;
		}
	}
}

void SwitcherData::loadSceneTriggers(obs_data_t *obj)
{
	switcher->sceneTriggers.clear();

	loadOldAutoStopStart(obj);

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
	obs_source_t *sceneSource = obs_weak_source_get_source(scene);
	const char *sceneName = obs_source_get_name(sceneSource);
	obs_data_set_string(obj, "scene", sceneName);
	obs_source_release(sceneSource);

	obs_data_set_int(obj, "triggerType", static_cast<int>(triggerType));
	obs_data_set_int(obj, "triggerAction", static_cast<int>(triggerAction));
	obs_data_set_double(obj, "duration", duration);

	obs_source_t *source = obs_weak_source_get_source(audioSource);
	const char *audioSourceName = obs_source_get_name(source);
	obs_data_set_string(obj, "audioSource", audioSourceName);
	obs_source_release(source);
}

void SceneTrigger::load(obs_data_t *obj)
{
	const char *sceneName = obs_data_get_string(obj, "scene");
	scene = GetWeakSourceByName(sceneName);

	triggerType = static_cast<sceneTriggerType>(
		obs_data_get_int(obj, "triggerType"));
	triggerAction = static_cast<sceneTriggerAction>(
		obs_data_get_int(obj, "triggerAction"));
	duration = obs_data_get_double(obj, "duration");

	const char *audioSourceName = obs_data_get_string(obj, "audioSource");
	audioSource = GetWeakSourceByName(audioSourceName);
}

static inline void populateTriggers(QComboBox *list)
{
	AdvSceneSwitcher::addSelectionEntry(
		list,
		obs_module_text(
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
	AdvSceneSwitcher::addSelectionEntry(
		list,
		obs_module_text(
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
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.muteSource"));
}

SceneTriggerWidget::SceneTriggerWidget(QWidget *parent, SceneTrigger *s)
	: SwitchWidget(parent, s, false, false)
{
	triggers = new QComboBox();
	actions = new QComboBox();
	duration = new QDoubleSpinBox();
	audioSources = new QComboBox();

	duration->setMinimum(0.0);
	duration->setMaximum(99.000000);
	duration->setSuffix("s");

	QWidget::connect(triggers, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TriggerTypeChanged(int)));
	QWidget::connect(actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TriggerActionChanged(int)));
	QWidget::connect(duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(AudioSourceChanged(const QString &)));

	populateTriggers(triggers);
	populateActions(actions);
	AdvSceneSwitcher::populateAudioSelection(audioSources);

	if (s) {
		triggers->setCurrentIndex(static_cast<int>(s->triggerType));
		actions->setCurrentIndex(static_cast<int>(s->triggerAction));
		duration->setValue(s->duration);

		audioSources->setCurrentText(
			GetWeakSourceName(s->audioSource).c_str());

		if (s->triggerAction == sceneTriggerAction::MUTE_SOURCE) {
			audioSources->show();
		} else {
			audioSources->hide();
		}
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{triggers}}", triggers},
		{"{{actions}}", actions},
		{"{{audioSources}}", audioSources},
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
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->triggerType = static_cast<sceneTriggerType>(index);
}

void SceneTriggerWidget::TriggerActionChanged(int index)
{
	if (loading || !switchData) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		switchData->triggerAction =
			static_cast<sceneTriggerAction>(index);
	}

	if (switchData->triggerAction == sceneTriggerAction::MUTE_SOURCE) {
		audioSources->show();
	} else {
		audioSources->hide();
	}
}

void SceneTriggerWidget::DurationChanged(double dur)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}

void SceneTriggerWidget::AudioSourceChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->audioSource = GetWeakSourceByQString(text);
}
