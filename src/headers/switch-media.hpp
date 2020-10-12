#pragma once
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto media_func = 6;
constexpr auto default_priority_6 = media_func;

typedef enum {
	TIME_RESTRICTION_NONE,
	TIME_RESTRICTION_SHORTER,
	TIME_RESTRICTION_LONGER,
	TIME_RESTRICTION_REMAINING_SHORTER,
	TIME_RESTRICTION_REMAINING_LONGER,
} time_restriction;

struct MediaSwitch : SceneSwitcherEntry {
	OBSWeakSource source = nullptr;
	obs_media_state state = OBS_MEDIA_STATE_NONE;
	time_restriction restriction = TIME_RESTRICTION_NONE;
	int64_t time = 0;

	const char *getType() { return "media"; }
	bool initialized();
	bool valid();

	inline MediaSwitch(){};
	inline MediaSwitch(OBSWeakSource scene_, OBSWeakSource source_,
			   OBSWeakSource transition_, obs_media_state state_,
			   time_restriction restriction_, uint64_t time_,
			   bool usePreviousScene_)
		: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
		  source(source_),
		  state(state_),
		  restriction(restriction_),
		  time(time_)
	{
	}
};

class MediaSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	MediaSwitchWidget(MediaSwitch *s);
	MediaSwitch *getSwitchData();
	void setSwitchData(MediaSwitch *s);

	static void swapSwitchData(MediaSwitchWidget *s1,
				   MediaSwitchWidget *s2);

private slots:
	void SourceChanged(const QString &text);
	void StateChanged(int index);
	void TimeRestrictionChanged(int index);
	void TimeChanged(int time);

private:
	QComboBox *meidaSources;
	QComboBox *states;
	QComboBox *timeRestrictions;
	QSpinBox *time;

	QLabel *whenLabel;
	QLabel *stateLabel;
	QLabel *andLabel;
	QLabel *switchLabel;
	QLabel *usingLabel;

	MediaSwitch *switchData;
};
