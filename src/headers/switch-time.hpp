/******************************************************************************
    Note: Long-term goal is to remove this tab / file.
    Most functionality shall be moved to the Macro tab instead.

    So if you plan to make changes here, please consider applying them to the
    corresponding macro tab functionality instead.
******************************************************************************/
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
	static bool pause;
	timeTrigger trigger = ANY_DAY;
	QTime time = QTime(0, 0);

	const char *getType() { return "time"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
};

class TimeSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	TimeSwitchWidget(QWidget *parent, TimeSwitch *s);
	TimeSwitch *getSwitchData();
	void setSwitchData(TimeSwitch *s);

	static void swapSwitchData(TimeSwitchWidget *s1, TimeSwitchWidget *s2);

private slots:
	void TriggerChanged(int index);
	void TimeChanged(const QTime &time);

private:
	QComboBox *triggers;
	QTimeEdit *time;

	TimeSwitch *switchData;
};
