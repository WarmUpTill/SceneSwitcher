#pragma once
#include <QCheckBox>
#include "switch-generic.hpp"

constexpr auto exe_func = 3;
constexpr auto default_priority_3 = exe_func;

struct ExecutableSwitch : SceneSwitcherEntry {
	QString exe = "";
	bool inFocus = false;

	const char *getType() { return "exec"; }

	inline ExecutableSwitch(){};
	inline ExecutableSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
				const QString &exe_, bool inFocus_)
		: SceneSwitcherEntry(scene_, transition_),
		  exe(exe_),
		  inFocus(inFocus_)
	{
	}
};

class ExecutableSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	ExecutableSwitchWidget(ExecutableSwitch *s);
	ExecutableSwitch *getSwitchData();
	void setSwitchData(ExecutableSwitch *s);

	static void swapSwitchData(ExecutableSwitchWidget *s1,
				   ExecutableSwitchWidget *s2);

private slots:
	void ProcessChanged(const QString &text);
	void FocusChanged(int state);

private:
	QComboBox *processes;
	QCheckBox *requiresFocus;

	ExecutableSwitch *switchData;
};
