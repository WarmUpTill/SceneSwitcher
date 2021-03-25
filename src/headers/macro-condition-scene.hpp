#pragma once

#include "macro-entry.hpp"

class MacroConditionScene : public MacroCondition {
public:
	bool CheckCondition();
	bool Save();
	bool Load();
	OBSWeakSource _scene;
};

class MacroConditionSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneEdit(MacroConditionScene *entryData = nullptr);
	void UpdateEntryData();

private slots:
	void SceneChanged(const QString &text);

protected:
	QComboBox *_sceneSelection;
	MacroConditionScene *_entryData;

private:
	bool _loading = true;
};
