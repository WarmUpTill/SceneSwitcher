#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"

namespace advss {

class MacroActionSudioMode : public MacroAction {
public:
	MacroActionSudioMode(Macro *m) : MacroAction(m) {}
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
		SWAP_SCENE,
		SET_SCENE, // TODO: Remove in future version as the
			   // functionality moved to the scene switch action
		ENABLE_STUDIO_MODE,
		DISABLE_STUDIO_MODE,
	};
	Action _action = Action::SWAP_SCENE;
	SceneSelection _scene;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionStudioModeEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionStudioModeEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSudioMode> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionStudioModeEdit(
			parent, std::dynamic_pointer_cast<MacroActionSudioMode>(
					action));
	}

private slots:
	void ActionChanged(int value);
	void SceneChanged(const SceneSelection &);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	QComboBox *_actions;
	SceneSelectionWidget *_scenes;
	std::shared_ptr<MacroActionSudioMode> _entryData;

	bool _loading = true;
};

} // namespace advss
