#pragma once
#include "switch-generic.hpp"

enum class PauseType {
	Scene,
	Window,
};

enum class PauseTarget {
	All,
	Transition,
	Window,
	Executable,
	Region,
	Media,
	File,
	Random,
	Time,
	Idle,
	Sequence,
	Audio,
};

struct PauseEntry : SceneSwitcherEntry {
	PauseType pauseType = PauseType::Scene;
	PauseTarget pauseTarget = PauseTarget::All;
	std::string window = "";

	const char *getType() { return "pause"; }
	bool valid();

	inline PauseEntry(){};
	inline PauseEntry(OBSWeakSource scene_, PauseType pauseType_,
			  PauseTarget pauseTarget_, const std::string &window_)
		: SceneSwitcherEntry(scene_, nullptr),
		  pauseType(pauseType_),
		  pauseTarget(pauseTarget_),
		  window(window_)
	{
	}
};

class PauseEntryWidget : public SwitchWidget {
	Q_OBJECT

public:
	PauseEntryWidget(QWidget *parent, PauseEntry *s);
	PauseEntry *getSwitchData();
	void setSwitchData(PauseEntry *s);

	static void swapSwitchData(PauseEntryWidget *s1, PauseEntryWidget *s2);

private slots:
	void PauseTypeChanged(int index);
	void PauseTargetChanged(int index);
	void WindowChanged(const QString &text);

private:
	QComboBox *pauseTypes;
	QComboBox *pauseTargets;
	QComboBox *windows;

	PauseEntry *switchData;
};
