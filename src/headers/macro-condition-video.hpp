#pragma once
#include "macro.hpp"
#include "screenshot-helper.hpp"

#include <QWidget>
#include <QComboBox>
#include <chrono>

enum class VideoCondition {
	MATCH,
	DIFFER,
	HAS_NOT_CHANGED,
	HAS_CHANGED,
	NO_IMAGE,
	PATTERN,
};

class MacroConditionVideo : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	QImage GetMatchImage() { return _matchImage; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionVideo>();
	}
	void GetScreenshot();
	bool LoadImageFromFile();

	OBSWeakSource _videoSource;
	VideoCondition _condition = VideoCondition::MATCH;
	std::string _file = obs_module_text("AdvSceneSwitcher.enterPath");
	double _threshold = 0.8;

private:
	bool ScreenshotContainsPattern();
	bool Compare();

	std::unique_ptr<AdvSSScreenshotObj> _screenshotData = nullptr;
	QImage _matchImage;
	static bool _registered;
	static const std::string id;
};

class ThresholdSlider : public QWidget {
	Q_OBJECT

public:
	ThresholdSlider(QWidget *parent = 0);
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
	void SetFilePath(const QString &text);

private slots:
	void SourceChanged(const QString &text);
	void ConditionChanged(int cond);
	void FilePathChanged();
	void BrowseButtonClicked();
	void ThresholdChanged(double);
	void ShowMatchClicked();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_videoSelection;
	QComboBox *_condition;
	QLineEdit *_filePath;
	QPushButton *_browseButton;
	ThresholdSlider *_threshold;
	QPushButton *_showMatch;
	std::shared_ptr<MacroConditionVideo> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};
