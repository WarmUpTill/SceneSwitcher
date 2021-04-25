#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

class MacroConditionScene : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionScene>();
	}

	OBSWeakSource _scene;

private:
	static bool _registered;
	static const int id;
};

class MacroConditionSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneEdit(
		std::shared_ptr<MacroConditionScene> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSceneEdit(
			std::dynamic_pointer_cast<MacroConditionScene>(cond));
	}

private slots:
	void SceneChanged(const QString &text);

protected:
	QComboBox *_sceneSelection;
	std::shared_ptr<MacroConditionScene> _entryData;

private:
	bool _loading = true;
};
