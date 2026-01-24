#pragma once
#include "duration-control.hpp"
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"
#include "transition-selection.hpp"

namespace advss {

class MacroActionSceneVisibility : public MacroAction {
public:
	MacroActionSceneVisibility(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	SceneSelection _scene;
	SceneItemSelection _source;
	bool _updateTransition = false;
	TransitionSelection _transition;
	bool _updateDuration = false;
	Duration _duration;

	enum class Action {
		SHOW,
		HIDE,
		TOGGLE,
	};
	Action _action = Action::SHOW;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSceneVisibilityEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSceneVisibilityEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSceneVisibility> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSceneVisibilityEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSceneVisibility>(
				action));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SceneItemSelection &);
	void UpdateTransitionChanged(int);
	void TransitionChanged(const TransitionSelection &);
	void UpdateDurationChanged(int);
	void DurationChanged(const Duration &seconds);
	void ActionChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	SceneSelectionWidget *_scenes;
	SceneItemSelectionWidget *_sources;
	QCheckBox *_updateTransition;
	TransitionSelectionWidget *_transitions;
	QCheckBox *_updateDuration;
	DurationSelection *_duration;
	QHBoxLayout *_durationLayout;
	QComboBox *_actions;

	std::shared_ptr<MacroActionSceneVisibility> _entryData;
	bool _loading = true;
};

} // namespace advss
