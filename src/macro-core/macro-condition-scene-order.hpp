#pragma once
#include "macro.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"

#include <QWidget>
#include <QComboBox>

enum class SceneOrderCondition {
	ABOVE,
	BELOW,
	POSITION,
};

class MacroConditionSceneOrder : public MacroCondition {
public:
	MacroConditionSceneOrder(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionSceneOrder>(m);
	}

	SceneSelection _scene;
	SceneItemSelection _source;
	SceneItemSelection _source2;
	int _position = 0;
	SceneOrderCondition _condition = SceneOrderCondition::ABOVE;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionSceneOrderEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneOrderEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionSceneOrder> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSceneOrderEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionSceneOrder>(
				cond));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SceneItemSelection &);
	void Source2Changed(const SceneItemSelection &);
	void ConditionChanged(int cond);
	void PositionChanged(int cond);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	QComboBox *_conditions;
	SceneItemSelectionWidget *_sources;
	SceneItemSelectionWidget *_sources2;
	QSpinBox *_position;
	QLabel *_posInfo;

	std::shared_ptr<MacroConditionSceneOrder> _entryData;

private:
	void SetWidgetVisibility(bool showPos);
	bool _loading = true;
};
