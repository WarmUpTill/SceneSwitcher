#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "transition-selection.hpp"
#include "duration-control.hpp"

#include <QCheckBox>

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

	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionSwitchScene>(m);
	}

	enum class SceneType { PROGRAM, PREVIEW };
	SceneType _sceneType = SceneType::PROGRAM;
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
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	SceneSelectionWidget *_scenes;
	TransitionSelectionWidget *_transitions;
	DurationSelection *_duration;
	QCheckBox *_blockUntilTransitionDone;
	QComboBox *_sceneTypes;
	QHBoxLayout *_entryLayout;

	std::shared_ptr<MacroActionSwitchScene> _entryData;
	bool _loading = true;
};

} // namespace advss
