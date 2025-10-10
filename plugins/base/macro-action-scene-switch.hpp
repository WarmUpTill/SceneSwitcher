#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "transition-selection.hpp"
#include "duration-control.hpp"

#include <QCheckBox>

#include <obs.hpp>

namespace advss {

class MacroActionSwitchScene : public MacroAction {
public:
	MacroActionSwitchScene(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		SCENE_NAME,
		SCENE_LIST_NEXT,
		SCENE_LIST_PREVIOUS,
		SCENE_LIST_INDEX,
	};
	Action _action = Action::SCENE_NAME;

	enum class SceneType { PROGRAM, PREVIEW };
	SceneType _sceneType = SceneType::PROGRAM;

	OBSWeakCanvas _canvas;
	IntVariable _index = 1;
	SceneSelection _scene;
	TransitionSelection _transition;
	Duration _duration;
	bool _blockUntilTransitionDone = true;

private:
	bool WaitForTransition(OBSWeakSource &scene, OBSWeakSource &transition);

	static bool _registered;
	static const std::string id;
};

class MacroActionSwitchSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSwitchSceneEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSwitchScene> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSwitchSceneEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSwitchScene>(
				action));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void TransitionChanged(const TransitionSelection &);
	void DurationChanged(const Duration &seconds);
	void BlockUntilTransitionDoneChanged(int state);
	void SceneTypeChanged(int);
	void CanvasChanged(const OBSWeakCanvas &);
	void SceneSelectionCanvasChanged(const OBSWeakCanvas &);
	void ActionChanged(int);
	void IndexChanged(const NumberVariable<int> &);
	void UpdateSceneNameAtIndex();
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetupWidgetConnections() const;
	void SetWidgetData() const;
	void SetWidgetVisibility();

	QComboBox *_actions;
	VariableSpinBox *_index;
	SceneSelectionWidget *_scenes;
	TransitionSelectionWidget *_transitions;
	DurationSelection *_duration;
	QCheckBox *_blockUntilTransitionDone;
	QComboBox *_sceneTypes;
	CanvasSelection *_canvas;
	QLabel *_sceneNameAtIndex;
	QTimer *_updateSceneNameTimer;
	QHBoxLayout *_actionLayout;
	QHBoxLayout *_transitionLayout;
	QLabel *_notSupportedWarning;

	std::shared_ptr<MacroActionSwitchScene> _entryData;
	bool _loading = true;
};

} // namespace advss
