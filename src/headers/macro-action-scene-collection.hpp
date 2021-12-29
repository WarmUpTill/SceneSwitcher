#pragma once
#include "macro-action-edit.hpp"

class MacroActionSceneCollection : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSceneCollection>();
	}

	std::string _sceneCollection;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSceneCollectionEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSceneCollectionEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSceneCollection> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSceneCollectionEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSceneCollection>(
				action));
	}

private slots:
	void SceneCollectionChanged(const QString &text);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_sceneCollections;
	std::shared_ptr<MacroActionSceneCollection> _entryData;

private:
	bool _loading = true;
};
