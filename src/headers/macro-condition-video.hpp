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
};

class MacroConditionVideo : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
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

private:
	bool Compare();

	std::unique_ptr<AdvSSScreenshotObj> _screenshotData = nullptr;
	QImage _matchImage;
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
	void SetFilePath(const QString &text);

private slots:
	void SourceChanged(const QString &text);
	void ConditionChanged(int cond);
	void FilePathChanged();
	void BrowseButtonClicked();

protected:
	QComboBox *_videoSelection;
	QComboBox *_condition;
	QLineEdit *_filePath;
	QPushButton *_browseButton;
	std::shared_ptr<MacroConditionVideo> _entryData;

private:
	bool _loading = true;
};
