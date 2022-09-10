#pragma once
#include "macro.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"
#include "resizing-text-edit.hpp"
#include "regex-config.hpp"

#include <QSpinBox>
#include <QCheckBox>

class MacroConditionSceneTransform : public MacroCondition {
public:
	MacroConditionSceneTransform(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionSceneTransform>(m);
	}

	SceneSelection _scene;
	SceneItemSelection _source;
	RegexConfig _regex;
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
	void SourceChanged(const SceneItemSelection &);
	void GetSettingsClicked();
	void SettingsChanged();
	void RegexChanged(RegexConfig);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	SceneItemSelectionWidget *_sources;
	QPushButton *_getSettings;
	ResizingPlainTextEdit *_settings;
	RegexConfigWidget *_regex;

	std::shared_ptr<MacroConditionSceneTransform> _entryData;

private:
	bool _loading = true;
};
