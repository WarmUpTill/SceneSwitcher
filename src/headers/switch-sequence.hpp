#pragma once
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto round_trip_func = 1;
constexpr auto default_priority_1 = round_trip_func;

typedef enum {
	SECONDS,
	MINUTES,
	HOURS,
} delay_units;

struct SceneSequenceSwitch : SceneSwitcherEntry {
	OBSWeakSource startScene = nullptr;
	double delay = 0;
	int delayMultiplier = 1;

	const char *getType() { return "sequence"; }
	bool initialized();
	bool valid();
	void logSleep(int dur);

	inline SceneSequenceSwitch(){};
	inline SceneSequenceSwitch(OBSWeakSource startScene_,
				   OBSWeakSource scene_,
				   OBSWeakSource transition_, double delay_,
				   int delayMultiplier_, bool usePreviousScene_)
		: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
		  startScene(startScene_),
		  delay(delay_),
		  delayMultiplier(delayMultiplier_)
	{
	}
};

class SequenceWidget : public SwitchWidget {
	Q_OBJECT

public:
	SequenceWidget(SceneSequenceSwitch *s);
	SceneSequenceSwitch *getSwitchData();
	void setSwitchData(SceneSequenceSwitch *s);

	static void swapSwitchData(SequenceWidget *s1, SequenceWidget *s2);

	void UpdateDelay();

private slots:
	void DelayChanged(double delay);
	void DelayUnitsChanged(int idx);
	void StartSceneChanged(const QString &text);

private:
	QDoubleSpinBox *delay;
	QComboBox *delayUnits;
	QComboBox *startScenes;

	QLabel *whenLabel;
	QLabel *switchLabel;
	QLabel *afterLabel;
	QLabel *usingLabel;

	SceneSequenceSwitch *switchData;
};
