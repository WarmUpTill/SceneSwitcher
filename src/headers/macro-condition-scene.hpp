#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>
#include "duration-control.hpp"

enum class SceneType {
	CURRENT,
	PREVIOUS,
};

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
	SceneType _type;
	Duration _duration;

private:
	static bool _registered;
	static const int id;
};

class MacroConditionSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionScene> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSceneEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionScene>(cond));
	}

private slots:
	void SceneChanged(const QString &text);
	void TypeChanged(int value);
	void DurationChanged(double seconds);
	void DurationUnitChanged(DurationUnit unit);

protected:
	QComboBox *_sceneSelection;
	QComboBox *_sceneType;
	DurationSelection *_duration;
	std::shared_ptr<MacroConditionScene> _entryData;

private:
	bool _loading = true;
};
