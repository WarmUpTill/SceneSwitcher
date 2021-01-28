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
	STOP_RECORDING = 1,
	STOP_STREAMING = 2,
	START_RECORDING = 3,
	START_STREAMING = 4,
};

struct SceneTrigger : SceneSwitcherEntry {
	static bool pause;
	sceneTriggerType triggerType = sceneTriggerType::NONE;
	sceneTriggerAction triggerAction = sceneTriggerAction::NONE;
	double duration = 0;

	const char *getType() { return "trigger"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
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

private:
	QComboBox *triggers;
	QComboBox *actions;
	QDoubleSpinBox *duration;

	SceneTrigger *switchData;
};
