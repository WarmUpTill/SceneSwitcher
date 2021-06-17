#pragma once
#include <QSpinBox>
#include <QPlainTextEdit>
#include "macro-action-edit.hpp"

class MacroActionSceneTransform : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSceneTransform>();
	}
	std::string GetSettings();
	void SetSettings(std::string &);

	OBSWeakSource _scene;
	OBSWeakSource _source;
	struct obs_transform_info _info;
	struct obs_sceneitem_crop _crop;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSceneTransformEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSceneTransformEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSceneTransform> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSceneTransformEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSceneTransform>(
				action));
	}

private slots:
	void SceneChanged(const QString &text);
	void SourceChanged(const QString &text);
	void GetSettingsClicked();
	void SettingsChanged();

protected:
	QComboBox *_scenes;
	QComboBox *_sources;
	QPushButton *_getSettings;
	QPlainTextEdit *_settings;
	std::shared_ptr<MacroActionSceneTransform> _entryData;

private:
	bool _loading = true;
};
