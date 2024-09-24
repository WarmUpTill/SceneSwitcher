/******************************************************************************
    Note: Long-term goal is to remove this tab / file.
    Most functionality shall be moved to the Macro tab instead.

    So if you plan to make changes here, please consider applying them to the
    corresponding macro tab functionality instead.
******************************************************************************/
#pragma once
#include <QSpinBox>

#include "switch-generic.hpp"
#include "obs-module-helper.hpp"
#include "screenshot-helper.hpp"

namespace advss {

constexpr auto video_func = 9;

enum class videoSwitchType {
	MATCH,
	DIFFER,
	HAS_NOT_CHANGED,
	HAS_CHANGED,
};

struct VideoSwitch : virtual SceneSwitcherEntry {
	static bool pause;

	videoSwitchType condition = videoSwitchType::MATCH;
	OBSWeakSource videoSource = nullptr;
	std::string file = obs_module_text("AdvSceneSwitcher.enterPath");
	double duration = 0;
	bool ignoreInactiveSource = false;

	std::unique_ptr<Screenshot> screenshotData = nullptr;
	std::chrono::high_resolution_clock::time_point previousTime{};
	QImage matchImage;

	std::chrono::milliseconds currentMatchDuration{};

	const char *getType() { return "video"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
	void getScreenshot();
	bool loadImageFromFile();
	bool checkMatch();

	VideoSwitch(){};
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

	void UpdatePreviewTooltip();
	void SetFilePath(const QString &text);

private slots:
	void SourceChanged(const QString &text);
	void ConditionChanged(int cond);
	void DurationChanged(double dur);
	void FilePathChanged();
	void BrowseButtonClicked();
	void IgnoreInactiveChanged(int state);

private:
	QComboBox *videoSources;
	QComboBox *condition;
	QDoubleSpinBox *duration;
	QLineEdit *filePath;
	QPushButton *browseButton;
	QCheckBox *ignoreInactiveSource;

	VideoSwitch *switchData;
};

} // namespace advss
