#pragma once
#include "switch-generic.hpp"

constexpr auto video_func = 9;
constexpr auto default_priority_9 = video_func;

struct VideoSwitch : virtual SceneSwitcherEntry {
	static bool pause;
	OBSWeakSource videoSource = nullptr;

	const char *getType() { return "video"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	VideoSwitch(){};
	//VideoSwitch(const VideoSwitch &other);
	//VideoSwitch(VideoSwitch &&other);
	//~VideoSwitch();
	//VideoSwitch &operator=(const VideoSwitch &other);
	//VideoSwitch &operator=(VideoSwitch &&other) noexcept;
	friend void swap(VideoSwitch &first, VideoSwitch &second);
};

class VideoSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	VideoSwitchWidget(QWidget *parent, VideoSwitch *s);
	void UpdateVolmeterSource();
	VideoSwitch *getSwitchData();
	void setSwitchData(VideoSwitch *s);

	static void swapSwitchData(VideoSwitchWidget *as1,
				   VideoSwitchWidget *as2);

private slots:
	void SourceChanged(const QString &text);

private:
	QComboBox *videoSources;

	VideoSwitch *switchData;
};
