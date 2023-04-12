#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"
#include "variable-text-edit.hpp"

#include <QSpinBox>

namespace advss {

class MacroActionSceneTransform : public MacroAction {
public:
	MacroActionSceneTransform(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionSceneTransform>(m);
	}

	SceneSelection _scene;
	SceneItemSelection _source;
	StringVariable _settings = "";

private:
	void ApplySettings(const std::string &);
	std::string ConvertSettings();

	struct obs_transform_info _info = {};
	struct obs_sceneitem_crop _crop = {};

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
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SceneItemSelection &);
	void GetSettingsClicked();
	void SettingsChanged();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SceneSelectionWidget *_scenes;
	SceneItemSelectionWidget *_sources;
	QPushButton *_getSettings;
	VariableTextEdit *_settings;
	std::shared_ptr<MacroActionSceneTransform> _entryData;

private:
	bool _loading = true;
};

} // namespace advss
