#include "headers/advanced-scene-switcher.hpp"
#include <obs-module.h>

void SceneSwitcher::on_close_clicked()
{
	done(0);
}

void SceneSwitcher::on_startAtLaunch_toggled(bool value)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->startAtLaunch = value;
}

void SceneSwitcher::UpdateNonMatchingScene(const QString &name)
{
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->nonMatchingScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SceneSwitcher::on_noMatchDontSwitch_clicked()
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = NO_SWITCH;
	ui->noMatchSwitchScene->setEnabled(false);
}

void SceneSwitcher::on_noMatchSwitch_clicked()
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = SWITCH;
	ui->noMatchSwitchScene->setEnabled(true);
	UpdateNonMatchingScene(ui->noMatchSwitchScene->currentText());
}

void SceneSwitcher::on_noMatchRandomSwitch_clicked()
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = RANDOM_SWITCH;
	ui->noMatchSwitchScene->setEnabled(false);
}

void SceneSwitcher::on_noMatchSwitchScene_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateNonMatchingScene(text);
}

void SceneSwitcher::on_checkInterval_valueChanged(int value)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->interval = value;
}

void SceneSwitcher::SetStarted()
{
	ui->toggleStartButton->setText("Stop");
	ui->pluginRunningText->setText("Active");
}

void SceneSwitcher::SetStopped()
{
	ui->toggleStartButton->setText("Start");
	ui->pluginRunningText->setText("Inactive");
}

void SceneSwitcher::on_toggleStartButton_clicked()
{
	if (switcher->th && switcher->th->isRunning()) {
		switcher->Stop();
		SetStopped();
	} else {
		switcher->Start();
		SetStarted();
	}
}

void SceneSwitcher::closeEvent(QCloseEvent *)
{
	obs_frontend_save();
}

void SceneSwitcher::on_autoStopScenes_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateAutoStopScene(text);
}

void SceneSwitcher::on_autoStopSceneCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	if (!state) {
		ui->autoStopScenes->setDisabled(true);
		switcher->autoStopEnable = false;
	} else {
		ui->autoStopScenes->setDisabled(false);
		switcher->autoStopEnable = true;
	}
}

void SceneSwitcher::UpdateAutoStopScene(const QString &name)
{
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->autoStopScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SwitcherData::autoStopStreamAndRecording()
{
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	if (ws && autoStopScene == ws) {
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
		if (obs_frontend_recording_active())
			obs_frontend_recording_stop();
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);
}

void SceneSwitcher::on_verboseLogging_stateChanged(int state)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->verbose = state;
}

void SwitcherData::saveGeneralSettings(obs_data_t *obj)
{
	obs_data_set_int(obj, "interval", switcher->interval);

	std::string nonMatchingSceneName =
		GetWeakSourceName(switcher->nonMatchingScene);
	obs_data_set_string(obj, "non_matching_scene",
			    nonMatchingSceneName.c_str());
	obs_data_set_int(obj, "switch_if_not_matching",
			 switcher->switchIfNotMatching);

	obs_data_set_bool(obj, "active", !switcher->stop);

	std::string autoStopSceneName =
		GetWeakSourceName(switcher->autoStopScene);
	obs_data_set_bool(obj, "autoStopEnable", switcher->autoStopEnable);
	obs_data_set_string(obj, "autoStopSceneName",
			    autoStopSceneName.c_str());

	obs_data_set_bool(obj, "verbose", switcher->verbose);

	obs_data_set_int(obj, "priority0",
			 switcher->functionNamesByPriority[0]);
	obs_data_set_int(obj, "priority1",
			 switcher->functionNamesByPriority[1]);
	obs_data_set_int(obj, "priority2",
			 switcher->functionNamesByPriority[2]);
	obs_data_set_int(obj, "priority3",
			 switcher->functionNamesByPriority[3]);
	obs_data_set_int(obj, "priority4",
			 switcher->functionNamesByPriority[4]);
	obs_data_set_int(obj, "priority5",
			 switcher->functionNamesByPriority[5]);
	obs_data_set_int(obj, "priority6",
			 switcher->functionNamesByPriority[6]);
	obs_data_set_int(obj, "priority7",
			 switcher->functionNamesByPriority[7]);

	obs_data_set_int(obj, "threadPriority", switcher->threadPriority);
}

void SwitcherData::loadGeneralSettings(obs_data_t *obj)
{
	obs_data_set_default_int(obj, "interval", DEFAULT_INTERVAL);
	switcher->interval = obs_data_get_int(obj, "interval");

	obs_data_set_default_int(obj, "switch_if_not_matching", NO_SWITCH);
	switcher->switchIfNotMatching =
		(NoMatch)obs_data_get_int(obj, "switch_if_not_matching");
	std::string nonMatchingScene =
		obs_data_get_string(obj, "non_matching_scene");
	switcher->nonMatchingScene =
		GetWeakSourceByName(nonMatchingScene.c_str());

	switcher->stop = !obs_data_get_bool(obj, "active");

	std::string autoStopScene =
		obs_data_get_string(obj, "autoStopSceneName");
	switcher->autoStopEnable = obs_data_get_bool(obj, "autoStopEnable");
	switcher->autoStopScene = GetWeakSourceByName(autoStopScene.c_str());

	switcher->verbose = obs_data_get_bool(obj, "verbose");

	obs_data_set_default_int(obj, "priority0", DEFAULT_PRIORITY_0);
	obs_data_set_default_int(obj, "priority1", DEFAULT_PRIORITY_1);
	obs_data_set_default_int(obj, "priority2", DEFAULT_PRIORITY_2);
	obs_data_set_default_int(obj, "priority3", DEFAULT_PRIORITY_3);
	obs_data_set_default_int(obj, "priority4", DEFAULT_PRIORITY_4);
	obs_data_set_default_int(obj, "priority5", DEFAULT_PRIORITY_5);
	obs_data_set_default_int(obj, "priority6", DEFAULT_PRIORITY_6);
	obs_data_set_default_int(obj, "priority7", DEFAULT_PRIORITY_7);

	switcher->functionNamesByPriority[0] =
		(obs_data_get_int(obj, "priority0"));
	switcher->functionNamesByPriority[1] =
		(obs_data_get_int(obj, "priority1"));
	switcher->functionNamesByPriority[2] =
		(obs_data_get_int(obj, "priority2"));
	switcher->functionNamesByPriority[3] =
		(obs_data_get_int(obj, "priority3"));
	switcher->functionNamesByPriority[4] =
		(obs_data_get_int(obj, "priority4"));
	switcher->functionNamesByPriority[5] =
		(obs_data_get_int(obj, "priority5"));
	switcher->functionNamesByPriority[6] =
		(obs_data_get_int(obj, "priority6"));
	switcher->functionNamesByPriority[6] =
		(obs_data_get_int(obj, "priority7"));
	if (!switcher->prioFuncsValid()) {
		switcher->functionNamesByPriority[0] = (DEFAULT_PRIORITY_0);
		switcher->functionNamesByPriority[1] = (DEFAULT_PRIORITY_1);
		switcher->functionNamesByPriority[2] = (DEFAULT_PRIORITY_2);
		switcher->functionNamesByPriority[3] = (DEFAULT_PRIORITY_3);
		switcher->functionNamesByPriority[4] = (DEFAULT_PRIORITY_4);
		switcher->functionNamesByPriority[5] = (DEFAULT_PRIORITY_5);
		switcher->functionNamesByPriority[6] = (DEFAULT_PRIORITY_6);
		switcher->functionNamesByPriority[7] = (DEFAULT_PRIORITY_7);
	}

	obs_data_set_default_int(obj, "threadPriority",
				 QThread::NormalPriority);
	switcher->threadPriority = obs_data_get_int(obj, "threadPriority");
}
