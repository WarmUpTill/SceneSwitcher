#pragma once
#include <QSpinBox>
#include "macro-action-edit.hpp"

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
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSceneVisibility>();
	}

	OBSWeakSource _scene;
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
	void SceneChanged(const QString &text);
	void SourceChanged(const QString &text);
	void ActionChanged(int value);

protected:
	QComboBox *_scenes;
	QComboBox *_sources;
	QComboBox *_actions;
	std::shared_ptr<MacroActionSceneVisibility> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
