#pragma once
#include "switch-generic.hpp"

struct RandomSwitch : SceneSwitcherEntry {
	static bool pause;
	double delay = 0.0;

	const char *getType() { return "random"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	inline RandomSwitch() {}
	inline RandomSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			    double delay_)
		: SceneSwitcherEntry(scene_, transition_), delay(delay_)
	{
	}
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

private:
	QDoubleSpinBox *delay;

	RandomSwitch *switchData;
};
