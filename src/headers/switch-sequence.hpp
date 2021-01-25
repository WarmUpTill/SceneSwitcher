#pragma once
#include <QVBoxLayout>

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

	std::unique_ptr<SceneSequenceSwitch> extendedSequence = nullptr;

	const char *getType() { return "sequence"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	bool reduce();
	SceneSequenceSwitch *extend();
};

class SequenceWidget : public SwitchWidget {
	Q_OBJECT

public:
	SequenceWidget(QWidget *parent, SceneSequenceSwitch *s,
		       bool extendSequence = false);
	SceneSequenceSwitch *getSwitchData();
	void setSwitchData(SceneSequenceSwitch *s);

	static void swapSwitchData(SequenceWidget *s1, SequenceWidget *s2);

	void UpdateDelay();

private slots:
	void SceneChanged(const QString &text);
	void DelayChanged(double delay);
	void DelayUnitsChanged(int idx);
	void StartSceneChanged(const QString &text);
	void InterruptibleChanged(int state);
	void ExtendClicked();
	void ReduceClicked();

protected:
	QDoubleSpinBox *delay;
	QComboBox *delayUnits;
	QComboBox *startScenes;
	QCheckBox *interruptible;
	QVBoxLayout *extendSequenceLayout;
	QPushButton *extend;
	QPushButton *reduce;

	SceneSequenceSwitch *switchData;
};
