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

	// nullptr marks start point and reaching end of extended sequence
	SceneSequenceSwitch *activeSequence = nullptr;

	std::unique_ptr<SceneSequenceSwitch> extendedSequence = nullptr;

	const char *getType() { return "sequence"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj, bool saveExt = true);
	void load(obs_data_t *obj, bool saveExt = true);

	bool reduce();
	SceneSequenceSwitch *extend();

	bool checkMatch(OBSWeakSource currentScene, int &linger);
	bool checkDurationMatchInterruptible();
	void prepareUninterruptibleMatch(OBSWeakSource currentScene,
					 int &linger);
	void advanceActiveSequence();
	void logAdvanceSequence();
};

class SequenceWidget : public SwitchWidget {
	Q_OBJECT

public:
	SequenceWidget(QWidget *parent, SceneSequenceSwitch *s,
		       bool extendSequence = false,
		       bool editExtendMode = false);
	SceneSequenceSwitch *getSwitchData();
	void setSwitchData(SceneSequenceSwitch *s);

	static void swapSwitchData(SequenceWidget *s1, SequenceWidget *s2);

	void UpdateDelay();
	void UpdateExtendText();

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

	// I would prefer having a list of only widgets of type editExtendMode
	// but I am not sure how to best implement that using a QListWidget.
	//
	// So use edit button to bring up edit widget and
	// add a label to disaplay current extended sequence state.
	QLabel *extendText;

	SceneSequenceSwitch *switchData;
};
