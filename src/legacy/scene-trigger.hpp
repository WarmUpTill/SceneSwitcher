/******************************************************************************
    Note: Long-term goal is to remove this tab / file.
    Most functionality shall be moved to the Macro tab instead.

    So if you plan to make changes here, please consider applying them to the
    corresponding macro tab functionality instead.
******************************************************************************/
#pragma once

#include "switch-generic.hpp"
#include "duration-control.hpp"

enum class sceneTriggerType {
	NONE = 0,
	SCENE_ACTIVE = 1,
	SCENE_INACTIVE = 2,
	SCENE_LEAVE = 3,
};

enum class sceneTriggerAction {
	NONE = 0,
	START_RECORDING,
	PAUSE_RECORDING,
	UNPAUSE_RECORDING,
	STOP_RECORDING,

	START_STREAMING,
	STOP_STREAMING,

	START_REPLAY_BUFFER,
	STOP_REPLAY_BUFFER,

	MUTE_SOURCE,
	UNMUTE_SOURCE,

	START_SWITCHER,
	STOP_SWITCHER,

	START_VCAM,
	STOP_VCAM,
};

struct SceneTrigger : SceneSwitcherEntry {
	static bool pause;
	sceneTriggerType triggerType = sceneTriggerType::NONE;
	sceneTriggerAction triggerAction = sceneTriggerAction::NONE;
	Duration duration;
	OBSWeakSource audioSource = nullptr;

	const char *getType() { return "trigger"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	bool checkMatch(OBSWeakSource currentScene,
			OBSWeakSource previousScene);
	void performAction();
	void logMatch();
};

class SceneTriggerWidget : public SwitchWidget {
	Q_OBJECT

public:
	SceneTriggerWidget(QWidget *parent, SceneTrigger *s);
	SceneTrigger *getSwitchData();
	void setSwitchData(SceneTrigger *s);

	static void swapSwitchData(SceneTriggerWidget *s1,
				   SceneTriggerWidget *s2);

private slots:
	void TriggerTypeChanged(int index);
	void TriggerActionChanged(int index);
	void DurationChanged(const Duration &);
	void AudioSourceChanged(const QString &text);

private:
	QComboBox *triggers;
	QComboBox *actions;
	DurationSelection *duration;
	QComboBox *audioSources;

	SceneTrigger *switchData;
};
