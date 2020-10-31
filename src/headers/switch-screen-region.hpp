#pragma once
#include <QSpinBox>
#include "switch-generic.hpp"

constexpr auto screen_region_func = 4;
constexpr auto default_priority_4 = screen_region_func;

struct ScreenRegionSwitch : SceneSwitcherEntry {
	int minX = 0, minY = 0, maxX = 0, maxY = 0;

	const char *getType() { return "region"; }

	inline ScreenRegionSwitch(){};
	inline ScreenRegionSwitch(OBSWeakSource scene_,
				  OBSWeakSource transition_, int minX_,
				  int minY_, int maxX_, int maxY_)
		: SceneSwitcherEntry(scene_, transition_),
		  minX(minX_),
		  minY(minY_),
		  maxX(maxX_),
		  maxY(maxY_)
	{
	}
};

class ScreenRegionWidget : public SwitchWidget {
	Q_OBJECT

public:
	ScreenRegionWidget(ScreenRegionSwitch *s);
	ScreenRegionSwitch *getSwitchData();
	void setSwitchData(ScreenRegionSwitch *s);

	static void swapSwitchData(ScreenRegionWidget *s1,
				   ScreenRegionWidget *s2);

	void showFrame();
	void hideFrame();

private slots:
	void MinXChanged(int pos);
	void MinYChanged(int pos);
	void MaxXChanged(int pos);
	void MaxYChanged(int pos);

private:
	QSpinBox *minX;
	QSpinBox *minY;
	QSpinBox *maxX;
	QSpinBox *maxY;

	QFrame helperFrame;

	QLabel *cursorLabel;
	QLabel *xLabel;
	QLabel *switchLabel;
	QLabel *usingLabel;

	ScreenRegionSwitch *switchData;

	void drawFrame();
};
