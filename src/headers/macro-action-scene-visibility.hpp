#pragma once
#include <QSpinBox>
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"

enum class SceneVisibilityAction {
	SHOW,
	HIDE,
};

class MacroActionSceneVisibility : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSceneVisibility>();
	}

	SceneSelection _scene;
	OBSWeakSource _source;
	SceneVisibilityAction _action = SceneVisibilityAction::SHOW;

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
	void SourceChanged(const QString &text);
	void ActionChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	QComboBox *_sources;
	QComboBox *_actions;
	std::shared_ptr<MacroActionSceneVisibility> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
