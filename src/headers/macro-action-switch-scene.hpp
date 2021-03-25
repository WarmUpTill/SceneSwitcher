#pragma once
#include "macro-action-edit.hpp"

class MacroActionSwitchScene : public MacroAction {
public:
	bool PerformAction();
	bool Save();
	bool Load();
	OBSWeakSource _scene = nullptr;
};

class MacroActionSwitchSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSwitchSceneEdit(MacroActionSwitchScene *entryData = nullptr);
	void UpdateEntryData();

private slots:
	void SceneChanged(const QString &text);

protected:
	QComboBox *_sceneSelection;
	MacroActionSwitchScene *_entryData;

private:
	bool _loading = true;
};
