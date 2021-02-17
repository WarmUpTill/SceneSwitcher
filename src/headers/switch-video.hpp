#pragma once
#include "switch-generic.hpp"

constexpr auto video_func = 9;
constexpr auto default_priority_9 = video_func;

class AdvSSScreenshotObj : public QObject {
	Q_OBJECT

public:
	AdvSSScreenshotObj(obs_source_t *source);
	~AdvSSScreenshotObj() override;
	void Screenshot();
	void Download();
	void Copy();
	void MuxAndFinish();

	gs_texrender_t *texrender = nullptr;
	gs_stagesurf_t *stagesurf = nullptr;
	OBSWeakSource weakSource;
	std::string path;
	QImage image;
	uint32_t cx = 0;
	uint32_t cy = 0;
	std::thread th;

	int stage = 0;

public slots:
	void Save();
};

struct VideoSwitch : virtual SceneSwitcherEntry {
	static bool pause;
	OBSWeakSource videoSource = nullptr;
	std::string file = obs_module_text("AdvSceneSwitcher.enterPath");
	AdvSSScreenshotObj *screenshotData = nullptr;

	const char *getType() { return "video"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
	void GetScreenshot();
	bool checkMatch();

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
	VideoSwitch *getSwitchData();
	void setSwitchData(VideoSwitch *s);

	static void swapSwitchData(VideoSwitchWidget *as1,
				   VideoSwitchWidget *as2);

private slots:
	void SourceChanged(const QString &text);
	void FilePathChanged();
	void BrowseButtonClicked();

private:
	QComboBox *videoSources;
	QLineEdit *filePath;
	QPushButton *browseButton;

	VideoSwitch *switchData;
};
