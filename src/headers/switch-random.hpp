/******************************************************************************
    Note: Long-term goal is to remove this tab / file.
    Most functionality shall be moved to the Macro tab instead.

    So if you plan to make changes here, please consider applying them to the
    corresponding macro tab functionality instead.
******************************************************************************/
#pragma once
#include "switch-generic.hpp"
#include "duration-control.hpp"

struct RandomSwitch : SceneSwitcherEntry {
	static bool pause;
	double delay = 0.0;
	bool waitFullDuration = true;

	const char *getType() { return "random"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	static bool waiting;
	static OBSWeakSource waitScene;
	static Duration waitTime;
};

class RandomSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	RandomSwitchWidget(QWidget *parent, RandomSwitch *s);
	RandomSwitch *getSwitchData();
	void setSwitchData(RandomSwitch *s);

	static void swapSwitchData(RandomSwitchWidget *s1,
				   RandomSwitchWidget *s2);

private slots:
	void DelayChanged(double d);
	void WaitFullDurationChanged(int);

private:
	QDoubleSpinBox *delay;
	QCheckBox *waitFullDuration;

	RandomSwitch *switchData;
};
