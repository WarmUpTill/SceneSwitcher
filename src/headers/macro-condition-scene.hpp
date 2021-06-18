#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

enum class SceneType {
	CURRENT,
	PREVIOUS,
};

class MacroConditionScene : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionScene>();
	}

	OBSWeakSource _scene;
	SceneType _type;

private:
	static bool _registered;
	static const std::string id;
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
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_sceneSelection;
	QComboBox *_sceneType;
	std::shared_ptr<MacroConditionScene> _entryData;

private:
	bool _loading = true;
};
