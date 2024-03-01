#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"

#include <QSpinBox>

namespace advss {

enum class SceneOrderAction {
	MOVE_UP,
	MOVE_DOWN,
	MOVE_TOP,
	MOVE_BOTTOM,
	POSITION,
};

class MacroActionSceneOrder : public MacroAction {
public:
	MacroActionSceneOrder(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	SceneSelection _scene;
	SceneItemSelection _source;
	SceneOrderAction _action = SceneOrderAction::MOVE_UP;
	int _position = 0;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSceneOrderEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSceneOrderEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSceneOrder> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSceneOrderEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSceneOrder>(
				action));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SceneItemSelection &);
	void ActionChanged(int value);
	void PositionChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	SceneItemSelectionWidget *_sources;
	QComboBox *_actions;
	QSpinBox *_position;
	std::shared_ptr<MacroActionSceneOrder> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};

} // namespace advss
