#pragma once
#include "macro-action-edit.hpp"

class MacroActionSceneSwap : public MacroAction {
public:
	MacroActionSceneSwap(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionSceneSwap>(m);
	}

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSceneSwapEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSceneSwapEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSceneSwap> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSceneSwapEdit(
			parent, std::dynamic_pointer_cast<MacroActionSceneSwap>(
					action));
	}

protected:
	std::shared_ptr<MacroActionSceneSwap> _entryData;
};
