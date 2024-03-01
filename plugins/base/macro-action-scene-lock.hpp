#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"

namespace advss {

class MacroActionSceneLock : public MacroAction {
public:
	MacroActionSceneLock(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		LOCK,
		UNLOCK,
		TOGGLE,
	};
	Action _action = Action::LOCK;
	SceneSelection _scene;
	SceneItemSelection _source;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSceneLockEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSceneLockEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSceneLock> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSceneLockEdit(
			parent, std::dynamic_pointer_cast<MacroActionSceneLock>(
					action));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SceneItemSelection &);
	void ActionChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	SceneItemSelectionWidget *_sources;
	QComboBox *_actions;
	std::shared_ptr<MacroActionSceneLock> _entryData;

private:
	bool _loading = true;
};

} // namespace advss
