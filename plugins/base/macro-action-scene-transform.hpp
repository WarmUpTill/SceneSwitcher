#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "scene-item-selection.hpp"
#include "variable-text-edit.hpp"
#include "variable-spinbox.hpp"

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
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		RESET,
		ROTATE,
		FLIP_HORIZONTAL,
		FLIP_VERTICAL,
		FIT_TO_SCREEN,
		STRETCH_TO_SCREEN,
		CENTER_TO_SCREEN,
		CENTER_VERTICALLY,
		CENTER_HORIZONTALLY,
		MANUAL_TRANSFORM = 100,
	};

	Action _action = Action::RESET;
	SceneSelection _scene;
	SceneItemSelection _source;
	StringVariable _settings = "";
	DoubleVariable _rotation = 90.0;

private:
	void Transform(obs_scene_item *);
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
	void ActionChanged(int);
	void RotationChanged(const NumberVariable<double> &value);
	void GetSettingsClicked();
	void SettingsChanged();
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	SceneSelectionWidget *_scenes;
	SceneItemSelectionWidget *_sources;
	QComboBox *_action;
	VariableDoubleSpinBox *_rotation;
	QPushButton *_getSettings;
	VariableTextEdit *_settings;
	QHBoxLayout *_buttonLayout;

	std::shared_ptr<MacroActionSceneTransform> _entryData;
	bool _loading = true;
};

} // namespace advss
