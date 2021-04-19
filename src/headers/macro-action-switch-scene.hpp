#pragma once
#include "macro-action-edit.hpp"

class MacroActionSwitchScene : public MacroAction {
public:
	bool PerformAction();
	bool Save();
	bool Load();
	int GetId() { return id; };

	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSwitchScene>();
	}
	OBSWeakSource _scene = nullptr;

private:
	static bool _registered;
	static const int id;
};

class MacroActionSwitchSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSwitchSceneEdit(
		std::shared_ptr<MacroActionSwitchScene> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSwitchSceneEdit(
			std::dynamic_pointer_cast<MacroActionSwitchScene>(
				action));
	}

private slots:
	void SceneChanged(const QString &text);

protected:
	QComboBox *_sceneSelection;
	std::shared_ptr<MacroActionSwitchScene> _entryData;

private:
	bool _loading = true;
};
