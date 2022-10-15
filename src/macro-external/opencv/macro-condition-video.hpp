#pragma once
#include "opencv-helpers.hpp"
#include "threshold-slider.hpp"
#include "preview-dialog.hpp"
#include "area-selection.hpp"
#include "video-selection.hpp"

#include <macro.hpp>
#include <file-selection.hpp>
#include <screenshot-helper.hpp>

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QRect>

enum class VideoCondition {
	MATCH,
	DIFFER,
	HAS_NOT_CHANGED,
	HAS_CHANGED,
	NO_IMAGE,
	PATTERN,
	OBJECT,
	BRIGHTNESS,
};

class MacroConditionVideo : public MacroCondition {
public:
	MacroConditionVideo(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	QImage GetMatchImage() { return _matchImage; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionVideo>(m);
	}
	void GetScreenshot(bool blocking = false);
	bool LoadImageFromFile();
	bool LoadModelData(std::string &path);
	std::string GetModelDataPath() { return _modelDataPath; }
	void ResetLastMatch() { _lastMatchResult = false; }
	double GetCurrentBrightness() { return _currentBrigthness; }

	VideoSelection _video;
	VideoCondition _condition = VideoCondition::MATCH;
	std::string _file = obs_module_text("AdvSceneSwitcher.enterPath");
	// Enabling this will reduce matching latency, but slow down the
	// the condition checks of all macros overall.
	//
	// If not set the screenshot will be gathered in one interval and
	// checked in the next one.
	// If set both operations will happen in the same interval.
	bool _blockUntilScreenshotDone = false;
	bool _useAlphaAsMask = false;
	bool _usePatternForChangedCheck = false;
	PatternMatchData _patternData;
	double _patternThreshold = 0.8;
	double _brightnessThreshold = 0.5;
	cv::CascadeClassifier _objectCascade;
	double _scaleFactor = 1.1;
	int _minNeighbors = minMinNeighbors;
	advss::Size _minSize{0, 0};
	advss::Size _maxSize{0, 0};

	bool _checkAreaEnable = false;
	advss::Area _checkArea{0, 0, 0, 0};

	bool _throttleEnabled = false;
	int _throttleCount = 3;

private:
	bool OutputChanged();
	bool ScreenshotContainsPattern();
	bool ScreenshotContainsObject();
	bool CheckBrightnessThreshold();
	bool Compare();
	bool CheckShouldBeSkipped();

	bool _getNextScreenshot = true;
	ScreenshotHelper _screenshotData;
	QImage _matchImage;
	std::string _modelDataPath =
		obs_get_module_data_path(obs_current_module()) +
		std::string(
			"/res/cascadeClassifiers/haarcascade_frontalface_alt.xml");
	bool _lastMatchResult = false;
	int _runCount = 0;
	double _currentBrigthness = 0.;

	static bool _registered;
	static const std::string id;
};

class MacroConditionVideoEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionVideoEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionVideo> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionVideoEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionVideo>(cond));
	}

	void UpdatePreviewTooltip();

private slots:
	void VideoSelectionChanged(const VideoSelection &);
	void ConditionChanged(int cond);
	void ReduceLatencyChanged(int value);

	void ImagePathChanged(const QString &text);
	void ImageBrowseButtonClicked();

	void UsePatternForChangedCheckChanged(int value);
	void PatternThresholdChanged(double);
	void UseAlphaAsMaskChanged(int value);

	void BrightnessThresholdChanged(double);

	void ModelPathChanged(const QString &text);
	void ObjectScaleThresholdChanged(double);
	void MinNeighborsChanged(int value);
	void MinSizeChanged(advss::Size value);
	void MaxSizeChanged(advss::Size value);

	void CheckAreaEnableChanged(int value);
	void CheckAreaChanged(advss::Area);
	void CheckAreaChanged(QRect area);
	void SelectAreaClicked();

	void ThrottleEnableChanged(int value);
	void ThrottleCountChanged(int value);
	void ShowMatchClicked();

	void UpdateCurrentBrightness();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	VideoSelectionWidget *_videoSelection;
	QComboBox *_condition;

	QCheckBox *_reduceLatency;

	QCheckBox *_usePatternForChangedCheck;
	FileSelection *_imagePath;

	ThresholdSlider *_patternThreshold;
	QCheckBox *_useAlphaAsMask;

	ThresholdSlider *_brightnessThreshold;
	QLabel *_currentBrightness;

	FileSelection *_modelDataPath;
	QHBoxLayout *_modelPathLayout;
	ThresholdSlider *_objectScaleThreshold;
	QHBoxLayout *_neighborsControlLayout;
	QSpinBox *_minNeighbors;
	QLabel *_minNeighborsDescription;
	QHBoxLayout *_sizeLayout;
	SizeSelection *_minSize;
	SizeSelection *_maxSize;

	QHBoxLayout *_checkAreaControlLayout;
	QCheckBox *_checkAreaEnable;
	AreaSelection *_checkArea;
	QPushButton *_selectArea;

	QHBoxLayout *_throttleControlLayout;
	QCheckBox *_throttleEnable;
	QSpinBox *_throttleCount;

	QPushButton *_showMatch;
	PreviewDialog _previewDialog;

	std::shared_ptr<MacroConditionVideo> _entryData;

private:
	QTimer _updateBrightnessTimer;

	void SetWidgetVisibility();
	bool _loading = true;
};
