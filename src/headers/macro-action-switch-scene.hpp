#pragma once
#include "macro-action-edit.hpp"
#include "switch-generic.hpp"

class MacroActionSwitchScene : public MacroAction, public SceneSwitcherEntry {
public:
	bool PerformAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };

	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSwitchScene>();
	}

private:
	const char *getType() { return "MacroActionSwitchScene"; }
	static bool _registered;
	static const int id;
};

class MacroActionSwitchSceneEdit : public SwitchWidget {
	Q_OBJECT

public:
	MacroActionSwitchSceneEdit(
		std::shared_ptr<MacroActionSwitchScene> entryData = nullptr);
	static QWidget *Create(std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSwitchSceneEdit(
			std::dynamic_pointer_cast<MacroActionSwitchScene>(
				action));
	}

protected:
	std::shared_ptr<MacroActionSwitchScene> _entryData;
};
