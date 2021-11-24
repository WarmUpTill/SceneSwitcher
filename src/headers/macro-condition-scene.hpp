#pragma once
#include "macro.hpp"
#include "scene-selection.hpp"

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>

enum class SceneType {
	CURRENT,
	PREVIOUS,
	CHANGED,
	NOTCHANGED,
};

class MacroConditionScene : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionScene>();
	}

	// This option is to be removed in a future version and be replaced by
	// transition specific conditions
	bool _waitForTransition = true;

	SceneSelection _scene;
	SceneType _type;

private:
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
	void WaitForTransitionChanged(int state);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	QComboBox *_sceneType;
	QCheckBox *_waitForTransition;
	std::shared_ptr<MacroConditionScene> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};
