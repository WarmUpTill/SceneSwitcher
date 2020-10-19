#pragma once
#include "switch-generic.hpp"

struct RandomSwitch : SceneSwitcherEntry {
	double delay = 0.0;

	const char *getType() { return "random"; }

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
	RandomSwitchWidget(RandomSwitch *s);
	RandomSwitch *getSwitchData();
	void setSwitchData(RandomSwitch *s);

	static void swapSwitchData(RandomSwitchWidget *s1,
				   RandomSwitchWidget *s2);

private slots:
	void DelayChanged(double d);

private:
	QDoubleSpinBox *delay;

	QLabel *switchLabel;
	QLabel *usingLabel;
	QLabel *forLabel;

	RandomSwitch *switchData;
};
