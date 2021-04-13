#pragma once
#include "macro-action-edit.hpp"

class MacroActionSwitchScene : public MacroAction {
public:
	bool PerformAction();
	bool Save();
	bool Load();
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSwitchScene>();
	}
	OBSWeakSource _scene = nullptr;
};

class MacroActionSwitchSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSwitchSceneEdit(MacroActionSwitchScene *entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create() { return new MacroActionSwitchSceneEdit(); }

private slots:
	void SceneChanged(const QString &text);

protected:
	QComboBox *_sceneSelection;
	MacroActionSwitchScene *_entryData;

private:
	bool _loading = true;
};
