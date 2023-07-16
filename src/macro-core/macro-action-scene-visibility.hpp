#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"

#include <QSpinBox>

namespace advss {

class MacroActionSceneVisibility : public MacroAction {
public:
	MacroActionSceneVisibility(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionSceneVisibility>(m);
	}

	SceneSelection _scene;
	SceneItemSelection _source;

	enum class Action {
		SHOW,
		HIDE,
		TOGGLE,
	};
	Action _action = Action::SHOW;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSceneVisibilityEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSceneVisibilityEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSceneVisibility> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSceneVisibilityEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSceneVisibility>(
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
	std::shared_ptr<MacroActionSceneVisibility> _entryData;

private:
	bool _loading = true;
};

} // namespace advss
