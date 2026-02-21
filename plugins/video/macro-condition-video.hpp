#pragma once
#include "opencv-helpers.hpp"
#include "area-selection.hpp"
#include "parameter-wrappers.hpp"
#include "preview-dialog.hpp"

#include <macro-condition-edit.hpp>
#include <file-selection.hpp>
#include <screenshot-helper.hpp>
#include <slider-spinbox.hpp>
#include <variable-line-edit.hpp>
#include <variable-text-edit.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QRect>
#include <QWidget>

namespace advss {

class PreviewDialog;

class MacroConditionVideo : public QObject, public MacroCondition {
	Q_OBJECT

public:
	MacroConditionVideo(Macro *m);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionVideo>(m);
	}
	QImage GetMatchImage() const { return _matchImage; };
	void GetScreenshot(bool blocking = false);
	bool LoadImageFromFile();
	void ResetLastMatch() { _lastMatchResult = false; }
	double GetCurrentBrightness() const { return _currentBrightness; }
	void SetPageSegMode(tesseract::PageSegMode);
	bool SetLanguageCode(const std::string &);
	bool SetTesseractBaseDir(const std::string &);

	void SetCondition(VideoCondition);
	VideoCondition GetCondition() const { return _condition; }
	void SetupTempVars();

	VideoInput _video;
	std::string _file = obs_module_text("AdvSceneSwitcher.enterPath");
	// Enabling this will reduce matching latency, but slow down the
	// the condition checks of all macros overall.
	//
	// If not set the screenshot will be gathered in one interval and
	// checked in the next one.
	// If set both operations will happen in the same interval.
	//
	// TODO: Remove this option in a future release as it has become
	// superfluous with "short circuit" evaluation.
	bool _blockUntilScreenshotDone = true;
	NumberVariable<double> _brightnessThreshold = 0.5;
	PatternMatchParameters _patternMatchParameters;
	ObjDetectParameters _objMatchParameters;
	OCRParameters _ocrParameters;
	ColorParameters _colorParameters;
	AreaParameters _areaParameters;
	// TODO: Remove this option in a future release as it has become
	// superfluous with "short circuit" evaluation.
	bool _throttleEnabled = false;
	int _throttleCount = 3;

signals:
	void InputFileChanged();

private:
	bool FileInputIsUpToDate() const;

	bool OutputChanged();
	bool ScreenshotContainsPattern();
	bool ScreenshotContainsObject();
	bool CheckBrightnessThreshold();
	bool CheckOCR();
	bool CheckColor();
	bool Compare();
	bool CheckShouldBeSkipped();

	VideoCondition _condition = VideoCondition::MATCH;

	bool _getNextScreenshot = true;
	Screenshot _screenshotData;
	QImage _matchImage;
	PatternImageData _patternImageData;

	bool _lastMatchResult = false;
	int _runCount = 0;

	double _currentBrightness = 0.;

	std::string _loadedFile;
	QDateTime _loadedFileLastModified;

	static bool _registered;
	static const std::string id;
};

class BrightnessEdit : public QWidget {
	Q_OBJECT

public:
	BrightnessEdit(QWidget *parent,
		       const std::shared_ptr<MacroConditionVideo> &);

private slots:
	void BrightnessThresholdChanged(const NumberVariable<double> &);
	void UpdateCurrentBrightness();

private:
	SliderSpinBox *_threshold;
	QLabel *_current;
	QTimer _timer;

	std::shared_ptr<MacroConditionVideo> _entryData;
	bool _loading = true;
};

class OCREdit : public QWidget {
	Q_OBJECT

public:
	OCREdit(QWidget *parent, PreviewDialog *,
		const std::shared_ptr<MacroConditionVideo> &);

private slots:
	void SelectColorClicked();
	void ColorThresholdChanged(const NumberVariable<double> &);
	void MatchTextChanged();
	void RegexChanged(const RegexConfig &conf);
	void PageSegModeChanged(int);
	void TesseractBaseDirChanged(const QString &);
	void LanguageChanged();
	void UseConfigChanged(int);
	void ConfigFileChanged(const QString &);

private:
	void SetupColorLabel(const QColor &);

	VariableTextEdit *_matchText;
	RegexConfigWidget *_regex;
	QLabel *_textColor;
	QPushButton *_selectColor;
	SliderSpinBox *_colorThreshold;
	QComboBox *_pageSegMode;
	FileSelection *_tesseractBaseDir;
	VariableLineEdit *_languageCode;
	QCheckBox *_useConfig;
	FileSelection *_configFile;
	QPushButton *_openConfigFile;
	QPushButton *_reloadConfig;
	QHBoxLayout *_configLayout;

	PreviewDialog *_previewDialog;

	std::shared_ptr<MacroConditionVideo> _entryData;
	bool _loading = true;
};

class ObjectDetectEdit : public QWidget {
	Q_OBJECT

public:
	ObjectDetectEdit(QWidget *parent, PreviewDialog *,
			 const std::shared_ptr<MacroConditionVideo> &);

private slots:
	void ModelPathChanged(const QString &text);
	void ObjectScaleThresholdChanged(const NumberVariable<double> &);
	void MinNeighborsChanged(int value);
	void MinSizeChanged(Size value);
	void MaxSizeChanged(Size value);

private:
	FileSelection *_modelDataPath;
	SliderSpinBox *_objectScaleThreshold;
	QSpinBox *_minNeighbors;
	QLabel *_minNeighborsDescription;
	SizeSelection *_minSize;
	SizeSelection *_maxSize;

	PreviewDialog *_previewDialog;

	std::shared_ptr<MacroConditionVideo> _entryData;
	bool _loading = true;
};

class ColorEdit : public QWidget {
	Q_OBJECT

public:
	ColorEdit(QWidget *parent,
		  const std::shared_ptr<MacroConditionVideo> &);

private slots:
	void SelectColorClicked();
	void MatchThresholdChanged(const NumberVariable<double> &);
	void ColorThresholdChanged(const NumberVariable<double> &);

private:
	void SetupColorLabel(const QColor &);

	SliderSpinBox *_matchThreshold;
	SliderSpinBox *_colorThreshold;
	QLabel *_color;
	QPushButton *_selectColor;

	std::shared_ptr<MacroConditionVideo> _entryData;
	bool _loading = true;
};

class AreaEdit : public QWidget {
	Q_OBJECT

public:
	AreaEdit(QWidget *parent, PreviewDialog *,
		 const std::shared_ptr<MacroConditionVideo> &);

private slots:
	void CheckAreaEnableChanged(int value);
	void CheckAreaChanged(Area);
	void CheckAreaChanged(QRect area);
	void SelectAreaClicked();

signals:
	void Resized();

private:
	void SetWidgetVisibility();

	QCheckBox *_checkAreaEnable;
	AreaSelection *_checkArea;
	QPushButton *_selectArea;

	PreviewDialog *_previewDialog;

	std::shared_ptr<MacroConditionVideo> _entryData;
	bool _loading = true;
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

private slots:
	void VideoInputTypeChanged(int);
	void SourceChanged(const SourceSelection &);
	void SceneChanged(const SceneSelection &);
	void ConditionChanged(int cond);
	void ReduceLatencyChanged(int value);

	void ImagePathChanged(const QString &text);
	void ImageBrowseButtonClicked();

	void UsePatternForChangedCheckChanged(int value);
	void PatternThresholdChanged(const NumberVariable<double> &);
	void UseAlphaAsMaskChanged(int value);
	void PatternMatchModeChanged(int value);

	void ThrottleEnableChanged(int value);
	void ThrottleCountChanged(int value);
	void ShowMatchClicked();

	void SetWidgetVisibility();
	void Resize();

signals:
	void VideoSelectionChanged(const VideoInput &);
	void HeaderInfoChanged(const QString &);

private:
	void UpdatePreviewTooltip();
	void HandleVideoInputUpdate();
	void SetupPreviewDialogParams();

	QComboBox *_videoInputTypes;
	SceneSelectionWidget *_scenes;
	SourceSelectionWidget *_sources;
	QComboBox *_condition;

	QCheckBox *_reduceLatency;

	QCheckBox *_usePatternForChangedCheck;
	FileSelection *_imagePath;

	SliderSpinBox *_patternThreshold;
	QCheckBox *_useAlphaAsMask;
	QHBoxLayout *_patternMatchModeLayout;
	QComboBox *_patternMatchMode;

	QPushButton *_showMatch;
	PreviewDialog _previewDialog;

	BrightnessEdit *_brightness;
	OCREdit *_ocr;
	ObjectDetectEdit *_objectDetect;
	ColorEdit *_color;
	AreaEdit *_area;

	QHBoxLayout *_throttleControlLayout;
	QCheckBox *_throttleEnable;
	QSpinBox *_throttleCount;

	std::shared_ptr<MacroConditionVideo> _entryData;
	bool _loading = true;
};

} // namespace advss
