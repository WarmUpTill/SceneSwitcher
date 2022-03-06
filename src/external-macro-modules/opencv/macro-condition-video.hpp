#pragma once
#include "opencv-helpers.hpp"
#include "threshold-slider.hpp"
#include "video-match-dialog.hpp"
#include "area-selection.hpp"

#include <macro.hpp>
#include <file-selection.hpp>
#include <screenshot-helper.hpp>

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>

enum class VideoCondition {
	MATCH,
	DIFFER,
	HAS_NOT_CHANGED,
	HAS_CHANGED,
	NO_IMAGE,
	PATTERN,
	OBJECT,
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
	void GetScreenshot();
	bool LoadImageFromFile();
	bool LoadModelData(std::string &path);
	std::string GetModelDataPath() { return _modelDataPath; }
	void ResetLastMatch() { _lastMatchResult = false; }

	OBSWeakSource _videoSource;
	VideoCondition _condition = VideoCondition::MATCH;
	std::string _file = obs_module_text("AdvSceneSwitcher.enterPath");
	bool _useAlphaAsMask = false;
	bool _usePatternForChangedCheck = false;
	PatternMatchData _patternData;
	double _patternThreshold = 0.8;
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
	void SourceChanged(const QString &text);
	void ConditionChanged(int cond);
	void ImagePathChanged(const QString &text);
	void ImageBrowseButtonClicked();
	void UsePatternForChangedCheckChanged(int value);
	void PatternThresholdChanged(double);
	void UseAlphaAsMaskChanged(int value);

	void ModelPathChanged(const QString &text);
	void ObjectScaleThresholdChanged(double);
	void MinNeighborsChanged(int value);
	void MinSizeChanged(advss::Size value);
	void MaxSizeChanged(advss::Size value);

	void CheckAreaEnableChanged(int value);
	void CheckAreaChanged(advss::Area);

	void ThrottleEnableChanged(int value);
	void ThrottleCountChanged(int value);
	void ShowMatchClicked();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_videoSelection;
	QComboBox *_condition;

	QCheckBox *_usePatternForChangedCheck;
	FileSelection *_imagePath;
	ThresholdSlider *_patternThreshold;
	QCheckBox *_useAlphaAsMask;

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

	QHBoxLayout *_throttleControlLayout;
	QCheckBox *_throttleEnable;
	QSpinBox *_throttleCount;

	QPushButton *_showMatch;
	ShowMatchDialog _matchDialog;

	std::shared_ptr<MacroConditionVideo> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};
