#pragma once
#include "macro-entry.hpp"
#include <QWidget>
#include <QComboBox>

class MacroConditionScene : public MacroCondition {
public:
	bool CheckCondition();
	bool Save();
	bool Load();
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionScene>();
	}

	OBSWeakSource _scene;
};

class MacroConditionSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneEdit(MacroConditionScene *entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create() { return new MacroConditionSceneEdit(); }

private slots:
	void SceneChanged(const QString &text);

protected:
	QComboBox *_sceneSelection;
	MacroConditionScene *_entryData;

private:
	bool _loading = true;
};
