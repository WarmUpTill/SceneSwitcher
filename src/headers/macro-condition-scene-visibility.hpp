#pragma once
#include "macro.hpp"
#include "scene-selection.hpp"

#include <QComboBox>

enum class SceneVisibilityCondition {
	SHOWN,
	HIDDEN,
};

class MacroConditionSceneVisibility : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionSceneVisibility>();
	}

	SceneSelection _scene;
	OBSWeakSource _source;
	SceneVisibilityCondition _condition = SceneVisibilityCondition::SHOWN;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionSceneVisibilityEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSceneVisibilityEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionSceneVisibility> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSceneVisibilityEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionSceneVisibility>(
				cond));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const QString &text);
	void ConditionChanged(int cond);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	QComboBox *_sources;
	QComboBox *_conditions;

	std::shared_ptr<MacroConditionSceneVisibility> _entryData;

private:
	bool _loading = true;
};
