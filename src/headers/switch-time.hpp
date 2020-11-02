#pragma once
#include <QTimeEdit>

#include "switch-generic.hpp"

constexpr auto time_func = 7;
constexpr auto default_priority_7 = time_func;

typedef enum {
	ANY_DAY = 0,
	MONDAY = 1,
	TUSEDAY = 2,
	WEDNESDAY = 3,
	THURSDAY = 4,
	FRIDAY = 5,
	SATURDAY = 6,
	SUNDAY = 7,
	LIVE = 8
} timeTrigger;

struct TimeSwitch : SceneSwitcherEntry {
	timeTrigger trigger = ANY_DAY;
	QTime time = QTime();

	const char *getType() { return "time"; }

	inline TimeSwitch(){};
	inline TimeSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			  timeTrigger trigger_, QTime time_,
			  bool usePreviousScene_)
		: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
		  trigger(trigger_),
		  time(time_)
	{
	}
};

class TimeSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	TimeSwitchWidget(TimeSwitch *s);
	TimeSwitch *getSwitchData();
	void setSwitchData(TimeSwitch *s);

	static void swapSwitchData(TimeSwitchWidget *s1, TimeSwitchWidget *s2);

private slots:
	void TriggerChanged(int index);
	void TimeChanged(const QTime &time);

private:
	QComboBox *triggers;
	QTimeEdit *time;

	QLabel *atLabel;
	QLabel *switchLabel;
	QLabel *usingLabel;

	TimeSwitch *switchData;
};
