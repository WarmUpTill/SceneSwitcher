#pragma once
#include "switch-generic.hpp"

constexpr auto round_trip_func = 1;
constexpr auto default_priority_1 = round_trip_func;

typedef enum {
	SECONDS,
	MINUTES,
	HOURS,
} delay_units;

struct SceneSequenceSwitch : SceneSwitcherEntry {
	static bool pause;
	OBSWeakSource startScene = nullptr;
	double delay = 0;
	int delayMultiplier = 1;
	bool interruptible = false;
	unsigned int matchCount = 0;

	const char *getType() { return "sequence"; }
	bool initialized();
	bool valid();
	void logSleep(int dur);
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	inline SceneSequenceSwitch(){};
	inline SceneSequenceSwitch(SwitchTargetType targetType_,
				   OBSWeakSource startScene_,
				   OBSWeakSource scene_, SceneGroup *group_,
				   OBSWeakSource transition_, double delay_,
				   int delayMultiplier_, bool interruptible_,
				   bool usePreviousScene_)
		: SceneSwitcherEntry(targetType_, scene_, group_, transition_,
				     usePreviousScene_),
		  startScene(startScene_),
		  delay(delay_),
		  delayMultiplier(delayMultiplier_),
		  interruptible(interruptible_)
	{
	}
};

class SequenceWidget : public SwitchWidget {
	Q_OBJECT

public:
	SequenceWidget(QWidget *parent, SceneSequenceSwitch *s);
	SceneSequenceSwitch *getSwitchData();
	void setSwitchData(SceneSequenceSwitch *s);

	static void swapSwitchData(SequenceWidget *s1, SequenceWidget *s2);

	void UpdateDelay();

private slots:
	void DelayChanged(double delay);
	void DelayUnitsChanged(int idx);
	void StartSceneChanged(const QString &text);
	void InterruptibleChanged(int state);

private:
	QDoubleSpinBox *delay;
	QComboBox *delayUnits;
	QComboBox *startScenes;
	QCheckBox *interruptible;

	SceneSequenceSwitch *switchData;
};
