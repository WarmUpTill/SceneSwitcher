#pragma once
#include "macro-action-edit.hpp"
#include "switch-generic.hpp"
#include "duration-control.hpp"

class MacroActionSwitchScene : public MacroAction, public SceneSwitcherEntry {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };

	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSwitchScene>();
	}
	Duration _duration;

private:
	const char *getType() { return "MacroActionSwitchScene"; }

	static bool _registered;
	static const int id;
};

class MacroActionSwitchSceneEdit : public SwitchWidget {
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
	void DurationChanged(double seconds);

protected:
	DurationSelection *_duration;
	std::shared_ptr<MacroActionSwitchScene> _entryData;
};
