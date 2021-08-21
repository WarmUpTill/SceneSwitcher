#pragma once
#include "macro.hpp"
#include "scene-selection.hpp"

#include <QSpinBox>
#include <QPlainTextEdit>
#include <QCheckBox>

class MacroConditionSceneTransform : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionSceneTransform>();
	}

	SceneSelection _scene;
	OBSWeakSource _source;
	bool _regex = false;
	std::string _settings = "";

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionSceneTransformEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneTransformEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionSceneTransform> entryData =
			nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSceneTransformEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionSceneTransform>(
				cond));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const QString &text);
	void GetSettingsClicked();
	void SettingsChanged();
	void RegexChanged(int);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	QComboBox *_sources;
	QPushButton *_getSettings;
	QPlainTextEdit *_settings;
	QCheckBox *_regex;

	std::shared_ptr<MacroConditionSceneTransform> _entryData;

private:
	bool _loading = true;
};
