#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"

class MacroActionPreviewScene : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionPreviewScene>();
	}

	SceneSelection _scene;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionPreviewSceneEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionPreviewSceneEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionPreviewScene> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionPreviewSceneEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionPreviewScene>(
				action));
	}

private slots:
	void SceneChanged(const SceneSelection &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	std::shared_ptr<MacroActionPreviewScene> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
