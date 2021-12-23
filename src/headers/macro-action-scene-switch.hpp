#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "transition-selection.hpp"
#include "duration-control.hpp"

#include <QCheckBox>

class MacroActionSwitchScene : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };

	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSwitchScene>();
	}
	SceneSelection _scene;
	TransitionSelection _transition;
	Duration _duration;
	bool _blockUntilTransitionDone = true;

private:
	const char *getType() { return "MacroActionSwitchScene"; }

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
	void DurationChanged(double seconds);
	void BlockUntilTransitionDoneChanged(int state);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	TransitionSelectionWidget *_transitions;
	DurationSelection *_duration;
	QCheckBox *_blockUntilTransitionDone;

	std::shared_ptr<MacroActionSwitchScene> _entryData;

private:
	bool _loading = true;
};
