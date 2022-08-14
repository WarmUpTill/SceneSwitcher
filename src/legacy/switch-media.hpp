/******************************************************************************
    Note: Long-term goal is to remove this tab / file.
    Most functionality shall be moved to the Macro tab instead.

    So if you plan to make changes here, please consider applying them to the
    corresponding macro tab functionality instead.
******************************************************************************/
#pragma once
#include "switch-generic.hpp"

constexpr auto media_func = 6;

typedef enum {
	TIME_RESTRICTION_NONE,
	TIME_RESTRICTION_SHORTER,
	TIME_RESTRICTION_LONGER,
	TIME_RESTRICTION_REMAINING_SHORTER,
	TIME_RESTRICTION_REMAINING_LONGER,
} time_restriction;

struct MediaSwitch : SceneSwitcherEntry {
	static bool pause;
	OBSWeakSource source = nullptr;
	obs_media_state state = OBS_MEDIA_STATE_NONE;
	bool anyState = false;
	time_restriction restriction = TIME_RESTRICTION_NONE;
	int64_t time = 0;

	// Trigger scene change only once even if media state might trigger repeatedly
	bool matched = false;

	std::atomic_bool stopped = {false};
	std::atomic_bool ended = {false};

	// Workaround to enable use of "ended" to specify end of VLC playlist
	bool previousStateEnded = false;

	bool playedToEnd = false;

	const char *getType() { return "media"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	void clearSignalHandler();
	void resetSignalHandler();
	static void MediaStopped(void *data, calldata_t *);
	static void MediaEnded(void *data, calldata_t *);

	inline MediaSwitch(){};

	MediaSwitch(const MediaSwitch &other);
	MediaSwitch(MediaSwitch &&other) noexcept;
	~MediaSwitch();
	MediaSwitch &operator=(const MediaSwitch &other);
	MediaSwitch &operator=(MediaSwitch &&other) noexcept;
	friend void swap(MediaSwitch &first, MediaSwitch &second);
};

class MediaSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	MediaSwitchWidget(QWidget *parent, MediaSwitch *s);
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
	QComboBox *mediaSources;
	QComboBox *states;
	QComboBox *timeRestrictions;
	QSpinBox *time;

	MediaSwitch *switchData;
};
