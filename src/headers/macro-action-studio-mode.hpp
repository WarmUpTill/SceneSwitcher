#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"

enum class StudioModeAction {
	SWAP_SCENE,
	SET_SCENE,
	ENABLE_STUDIO_MODE,
	DISABLE_STUDIO_MODE,
};

class MacroActionSudioMode : public MacroAction {
public:
	MacroActionSudioMode(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionSudioMode>(m);
	}

	StudioModeAction _action = StudioModeAction::SWAP_SCENE;
	SceneSelection _scene;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSudioModeEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSudioModeEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSudioMode> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSudioModeEdit(
			parent, std::dynamic_pointer_cast<MacroActionSudioMode>(
					action));
	}

private slots:
	void ActionChanged(int value);
	void SceneChanged(const SceneSelection &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_actions;
	SceneSelectionWidget *_scenes;
	std::shared_ptr<MacroActionSudioMode> _entryData;

private:
	bool _loading = true;
};
