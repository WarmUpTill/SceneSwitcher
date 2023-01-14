#pragma once
#include "macro.hpp"
#include "scene-selection.hpp"

#include <QWidget>
#include <QComboBox>

enum class StudioModeCondition {
	STUDIO_MODE_ACTIVE,
	STUDIO_MODE_NOT_ACTIVE,
	PREVIEW_SCENE,
};

class MacroConditionStudioMode : public MacroCondition {
public:
	MacroConditionStudioMode(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionStudioMode>(m);
	}

	StudioModeCondition _condition =
		StudioModeCondition::STUDIO_MODE_ACTIVE;
	SceneSelection _scene;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionStudioModeEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionStudioModeEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionStudioMode> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionStudioModeEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionStudioMode>(
				cond));
	}
private slots:
	void ConditionChanged(int cond);
	void SceneChanged(const SceneSelection &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_condition;
	SceneSelectionWidget *_scenes;
	std::shared_ptr<MacroConditionStudioMode> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};
