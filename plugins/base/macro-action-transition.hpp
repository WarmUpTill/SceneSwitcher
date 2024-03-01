#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "transition-selection.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"

#include <QCheckBox>
#include <QHBoxLayout>

namespace advss {

class MacroActionTransition : public MacroAction {
public:
	MacroActionTransition(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Type {
		SCENE,
		SCENE_OVERRIDE,
		SOURCE_SHOW,
		SOURCE_HIDE,
	};

	Type _type = Type::SCENE;
	SceneItemSelection _source;
	SceneSelection _scene;
	bool _setDuration = true;
	bool _setTransitionType = true;
	TransitionSelection _transition;
	Duration _duration;

private:
	void SetSceneTransition();
	void SetTransitionOverride();
	void SetSourceTransition(bool);

	static bool _registered;
	static const std::string id;
};

class MacroActionTransitionEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionTransitionEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionTransition> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionTransitionEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionTransition>(
				action));
	}

private slots:
	void ActionChanged(int value);
	void SourceChanged(const SceneItemSelection &);
	void SceneChanged(const SceneSelection &);
	void SetTransitionChanged(int state);
	void SetDurationChanged(int state);
	void TransitionChanged(const TransitionSelection &);
	void DurationChanged(const Duration &seconds);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_actions;
	SceneItemSelectionWidget *_sources;
	SceneSelectionWidget *_scenes;
	QCheckBox *_setTransition;
	QCheckBox *_setDuration;
	TransitionSelectionWidget *_transitions;
	DurationSelection *_duration;
	QHBoxLayout *_transitionLayout;
	QHBoxLayout *_durationLayout;
	std::shared_ptr<MacroActionTransition> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
