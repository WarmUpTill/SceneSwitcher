#pragma once
#include "macro-condition-edit.hpp"
#include "scene-selection.hpp"
#include "regex-config.hpp"

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>

namespace advss {

class MacroConditionScene : public MacroCondition {
public:
	MacroConditionScene(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionScene>(m);
	}

	enum class Type {
		CURRENT = 10,
		PREVIOUS = 20,
		PREVIEW = 30,
		CHANGED = 40,
		NOT_CHANGED = 50,
		CURRENT_PATTERN = 60,
		PREVIOUS_PATTERN = 70,
		PREVIEW_PATTERN = 80,
	};
	void SetType(const Type &);
	Type GetType() const { return _type; }

	SceneSelection _scene;
	std::string _pattern = ".*Scene.*";
	RegexConfig _regex = RegexConfig(true);
	// During a transition "current" scene could either stand for the scene
	// being transitioned to or the scene still being transitioned away
	// from.
	bool _useTransitionTargetScene = false;

private:
	void SetupTempVars();

	Type _type = Type::CURRENT;
	std::chrono::high_resolution_clock::time_point _lastSceneChangeTime{};
	static bool _registered;
	static const std::string id;
};

class MacroConditionSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionScene> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSceneEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionScene>(cond));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void TypeChanged(int value);
	void PatternChanged();
	void UseTransitionTargetSceneChanged(int state);
	void RegexChanged(const RegexConfig &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	QComboBox *_sceneType;
	QLineEdit *_pattern;
	QCheckBox *_useTransitionTargetScene;
	RegexConfigWidget *_regex;
	std::shared_ptr<MacroConditionScene> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};

} // namespace advss
