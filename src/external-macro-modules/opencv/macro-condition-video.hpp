#pragma once
#include <macro.hpp>
#include <screenshot-helper.hpp>
#include <file-selection.hpp>

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QScrollArea>
#include <chrono>
#undef NO // MacOS macro that can conflict with OpenCV
#include <opencv2/opencv.hpp>

enum class VideoCondition {
	MATCH,
	DIFFER,
	HAS_NOT_CHANGED,
	HAS_CHANGED,
	NO_IMAGE,
	PATTERN,
	OBJECT,
};

struct PatternMatchData {
	cv::Mat4b rgbaPattern;
	cv::Mat3b rgbPattern;
	cv::Mat1b mask;
};

constexpr int minMinNeighbors = 3;
constexpr int maxMinNeighbors = 6;

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
	int _minSizeX = 0;
	int _minSizeY = 0;
	int _maxSizeX = 0;
	int _maxSizeY = 0;

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

class ThresholdSlider : public QWidget {
	Q_OBJECT

public:
	ThresholdSlider(double min = 0., double max = 1.,
			const QString &label = "threshold",
			const QString &description = "", QWidget *parent = 0);
	void SetDoubleValue(double);
public slots:
	void NotifyValueChanged(int value);
signals:
	void DoubleValueChanged(double value);

private:
	void SetDoubleValueText(double);
	QLabel *_value;
	QSlider *_slider;
	double _scale = 100.0;
	int _precision = 2;
};

class ShowMatchDialog : public QDialog {
	Q_OBJECT

public:
	ShowMatchDialog(QWidget *parent, MacroConditionVideo *_conditionData);
	virtual ~ShowMatchDialog();
	void ShowMatch();

private slots:
	void RedrawImage(QImage img);

private:
	void CheckForMatchLoop();
	QImage MarkMatch(QImage &screenshot);

	MacroConditionVideo *_conditionData;
	QScrollArea *_scrollArea;
	QLabel *_statusLabel;
	QLabel *_imageLabel;
	std::thread _thread;
	std::atomic_bool _stop = {false};
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
	void MinSizeXChanged(int value);
	void MinSizeYChanged(int value);
	void MaxSizeXChanged(int value);
	void MaxSizeYChanged(int value);

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
	QHBoxLayout *_minSizeControlLayout;
	QSpinBox *_minSizeX;
	QSpinBox *_minSizeY;
	QHBoxLayout *_maxSizeControlLayout;
	QSpinBox *_maxSizeX;
	QSpinBox *_maxSizeY;

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
