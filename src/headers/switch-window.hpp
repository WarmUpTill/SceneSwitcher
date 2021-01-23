#pragma once
#include "switch-generic.hpp"

constexpr auto window_title_func = 5;
constexpr auto default_priority_5 = window_title_func;

struct WindowSwitch : SceneSwitcherEntry {
	static bool pause;
	std::string window = "";
	bool fullscreen = false;
	bool maximized = false;
	bool focus = true;

	const char *getType() { return "window"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
};

class WindowSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	WindowSwitchWidget(QWidget *parent, WindowSwitch *s);
	WindowSwitch *getSwitchData();
	void setSwitchData(WindowSwitch *s);

	static void swapSwitchData(WindowSwitchWidget *s1,
				   WindowSwitchWidget *s2);

private slots:
	void WindowChanged(const QString &text);
	void FullscreenChanged(int state);
	void MaximizedChanged(int state);
	void FocusChanged(int state);

private:
	QComboBox *windows;
	QCheckBox *fullscreen;
	QCheckBox *maximized;
	QCheckBox *focused;

	WindowSwitch *switchData;
};
