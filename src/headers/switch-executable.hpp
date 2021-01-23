#pragma once
#include <QCheckBox>
#include "switch-generic.hpp"

constexpr auto exe_func = 3;
constexpr auto default_priority_3 = exe_func;

struct ExecutableSwitch : SceneSwitcherEntry {
	static bool pause;
	QString exe = "";
	bool inFocus = false;

	const char *getType() { return "exec"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
};

class ExecutableSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	ExecutableSwitchWidget(QWidget *parent, ExecutableSwitch *s);
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
