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
	case sceneTriggerAction::UNMUTE_SOURCE:
		actionName = "UNMUTE (" + GetWeakSourceName(audioSource) + ")";
		break;
	case sceneTriggerAction::START_SWITCHER:
		actionName = "START SCENE SWITCHER";
		break;
	case sceneTriggerAction::STOP_SWITCHER:
		actionName = "STOP SCENE SWITCHER";
		break;
	case sceneTriggerAction::START_VCAM:
		actionName = "START VIRTUAL CAMERA";
		break;
	case sceneTriggerAction::STOP_VCAM:
		actionName = "STOP VIRTUAL CAMERA";
		break;
	default:
		actionName = "UNKNOWN";
		break;
	}

	blog(LOG_INFO,
	     "scene '%s' in status '%s' triggering action '%s' after %f seconds",
	     GetWeakSourceName(scene).c_str(), statusName.c_str(),
	     actionName.c_str(), duration.seconds);
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
#ifdef REPLAYBUFFER_SUPPORTED
	case sceneTriggerAction::START_REPLAY_BUFFER:
		obs_frontend_replay_buffer_start();
		break;
	case sceneTriggerAction::STOP_REPLAY_BUFFER:
		obs_frontend_replay_buffer_stop();
		break;
#endif
#ifdef VCAM_SUPPORTED
	case sceneTriggerAction::START_VCAM:
		obs_frontend_start_virtualcam();
		break;
	case sceneTriggerAction::STOP_VCAM:
		obs_frontend_stop_virtualcam();
		break;
#endif
	default:
		blog(LOG_WARNING, "ignoring unexpected frontend action '%d'",
		     static_cast<int>(action));
		break;
	}
}

void muteThread(OBSWeakSource source, double delay, bool mute)
{
	long long mil = delay * 1000;
	std::this_thread::sleep_for(std::chrono::milliseconds(mil));

	auto s = obs_weak_source_get_source(source);
	obs_source_set_muted(s, mute);
	obs_source_release(s);
}

void statusThread(double delay, bool stop)
{
	long long mil = delay * 1000;
	std::this_thread::sleep_for(std::chrono::milliseconds(mil));

	if (stop) {
		switcher->Stop();
	} else {
		switcher->Start();
	}
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
	       triggerAction == sceneTriggerAction::STOP_REPLAY_BUFFER ||
	       triggerAction == sceneTriggerAction::START_VCAM ||
	       triggerAction == sceneTriggerAction::STOP_VCAM;
}

bool isAudioAction(sceneTriggerAction t)
{
	return t == sceneTriggerAction::MUTE_SOURCE ||
	       t == sceneTriggerAction::UNMUTE_SOURCE;
}

bool isSwitcherStatusAction(sceneTriggerAction t)
{
	return t == sceneTriggerAction::START_SWITCHER ||
	       t == sceneTriggerAction::STOP_SWITCHER;
}

void SceneTrigger::performAction()
{
	if (triggerAction == sceneTriggerAction::NONE) {
		return;
	}

	std::thread t;

	if (isFrontendAction(triggerAction)) {
		t = std::thread(frontEndActionThread, triggerAction,
				duration.seconds);
	} else if (isAudioAction(triggerAction)) {
		bool mute = triggerAction == sceneTriggerAction::MUTE_SOURCE;
		t = std::thread(muteThread, audioSource, duration.seconds,
				mute);
	} else if (isSwitcherStatusAction(triggerAction)) {
		bool stop = triggerAction == sceneTriggerAction::STOP_SWITCHER;
		t = std::thread(statusThread, duration.seconds, stop);
	} else {
		blog(LOG_WARNING, "ignoring unknown action '%d'",
		     static_cast<int>(triggerAction));
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
	if (SceneTrigger::pause) {
		return;
	}

	for (auto &t : sceneTriggers) {
		if (stop && !isSwitcherStatusAction(t.triggerAction)) {
			continue;
		}

		if (t.checkMatch(currentScene, previousScene)) {
			t.logMatch();
			t.performAction();
		}
	}
}

void SwitcherData::saveSceneTriggers(obs_data_t *obj)
{
	obs_data_array_t *triggerArray = obs_data_array_create();
	for (auto &s : sceneTriggers) {
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
	sceneTriggers.clear();

	obs_data_array_t *triggerArray = obs_data_get_array(obj, "triggers");
	size_t count = obs_data_array_count(triggerArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(triggerArray, i);

		sceneTriggers.emplace_back();
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
		if (!switcher->disableHints) {
			addPulse =
				PulseWidget(ui->triggerAdd, QColor(Qt::green));
		}
		ui->triggerHelp->setVisible(true);
	} else {
		ui->triggerHelp->setVisible(false);
	}
}

void SceneTrigger::save(obs_data_t *obj)
{
	obs_data_set_string(obj, "scene", GetWeakSourceName(scene).c_str());
	obs_data_set_int(obj, "triggerType", static_cast<int>(triggerType));
	obs_data_set_int(obj, "triggerAction", static_cast<int>(triggerAction));
	duration.Save(obj, "duration");
	obs_data_set_string(obj, "audioSource",
			    GetWeakSourceName(audioSource).c_str());
}

void SceneTrigger::load(obs_data_t *obj)
{
	const char *sceneName = obs_data_get_string(obj, "scene");
	scene = GetWeakSourceByName(sceneName);

	triggerType = static_cast<sceneTriggerType>(
		obs_data_get_int(obj, "triggerType"));
	triggerAction = static_cast<sceneTriggerAction>(
		obs_data_get_int(obj, "triggerAction"));
	duration.Load(obj, "duration");

	const char *audioSourceName = obs_data_get_string(obj, "audioSource");
	audioSource = GetWeakSourceByName(audioSourceName);
}

static inline void populateTriggers(QComboBox *list)
{
	addSelectionEntry(
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
	addSelectionEntry(
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
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.unmuteSource"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.startSwitcher"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.stopSwitcher"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.startVirtualCamera"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.sceneTriggerTab.sceneTriggerAction.stopVirtualCamera"));
}

SceneTriggerWidget::SceneTriggerWidget(QWidget *parent, SceneTrigger *s)
	: SwitchWidget(parent, s, false, false)
{
	triggers = new QComboBox();
	actions = new QComboBox();
	duration = new DurationSelection();
	audioSources = new QComboBox();

	QWidget::connect(triggers, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TriggerTypeChanged(int)));
	QWidget::connect(actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TriggerActionChanged(int)));
	QWidget::connect(duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));
	QWidget::connect(audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(AudioSourceChanged(const QString &)));

	populateTriggers(triggers);
	populateActions(actions);
	populateAudioSelection(audioSources);

	if (s) {
		triggers->setCurrentIndex(static_cast<int>(s->triggerType));
		actions->setCurrentIndex(static_cast<int>(s->triggerAction));
		duration->SetDuration(s->duration);

		audioSources->setCurrentText(
			GetWeakSourceName(s->audioSource).c_str());

		if (isAudioAction(s->triggerAction)) {
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

	if (isAudioAction(switchData->triggerAction)) {
		audioSources->show();
	} else {
		audioSources->hide();
	}
}

void SceneTriggerWidget::DurationChanged(double seconds)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration.seconds = seconds;
}

void SceneTriggerWidget::DurationUnitChanged(DurationUnit unit)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration.displayUnit = unit;
}

void SceneTriggerWidget::AudioSourceChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->audioSource = GetWeakSourceByQString(text);
}
