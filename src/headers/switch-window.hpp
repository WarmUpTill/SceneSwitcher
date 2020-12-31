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

	inline WindowSwitch() {}
	inline WindowSwitch(OBSWeakSource scene_, const char *window_,
			    OBSWeakSource transition_, bool fullscreen_,
			    bool maximized_, bool focus_)
		: SceneSwitcherEntry(scene_, transition_),
		  window(window_),
		  fullscreen(fullscreen_),
		  maximized(maximized_),
		  focus(focus_)
	{
	}
};

class WindowSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	WindowSwitchWidget(WindowSwitch *s);
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
