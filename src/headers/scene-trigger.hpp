#pragma once

#include "switch-generic.hpp"

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
};

struct SceneTrigger : SceneSwitcherEntry {
	static bool pause;
	sceneTriggerType triggerType = sceneTriggerType::NONE;
	sceneTriggerAction triggerAction = sceneTriggerAction::NONE;
	double duration = 0;
	OBSWeakSource audioSource = nullptr;

	const char *getType() { return "trigger"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	bool checkMatch(OBSWeakSource previousScene);
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
	void DurationChanged(double dur);
	void AudioSourceChanged(const QString &text);

private:
	QComboBox *triggers;
	QComboBox *actions;
	QDoubleSpinBox *duration;
	QComboBox *audioSources;

	SceneTrigger *switchData;
};
