#pragma once
#include "macro-condition-edit.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"

#include <QComboBox>

namespace advss {

class MacroConditionSceneVisibility : public MacroCondition {
public:
	MacroConditionSceneVisibility(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionSceneVisibility>(m);
	}

	enum class Condition {
		SHOWN,
		HIDDEN,
		CHANGED,
	};

	SceneSelection _scene;
	SceneItemSelection _source;
	Condition _condition = Condition::SHOWN;

private:
	std::vector<bool> _previousVisibility;

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
	void SourceChanged(const SceneItemSelection &);
	void ConditionChanged(int cond);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	SceneItemSelectionWidget *_sources;
	QComboBox *_conditions;

	std::shared_ptr<MacroConditionSceneVisibility> _entryData;

private:
	bool _loading = true;
};

} // namespace advss
