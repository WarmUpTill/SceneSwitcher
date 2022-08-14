#pragma once
#include "macro-action-edit.hpp"
#include "file-selection.hpp"

#include <QDoubleSpinBox>
#include <QLabel>

enum class PluginStateAction {
	STOP,
	NO_MATCH_BEHAVIOUR,
	IMPORT_SETTINGS,
};

class MacroActionPluginState : public MacroAction {
public:
	MacroActionPluginState(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionPluginState>(m);
	}

	PluginStateAction _action = PluginStateAction::STOP;
	int _value = 0;
	std::string _settingsPath;
	OBSWeakSource _scene;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionPluginStateEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionPluginStateEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionPluginState> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionPluginStateEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionPluginState>(
				action));
	}

private slots:
	void ActionChanged(int value);
	void ValueChanged(int value);
	void SceneChanged(const QString &text);
	void PathChanged(const QString &text);

protected:
	QComboBox *_actions;
	QComboBox *_values;
	QComboBox *_scenes;
	FileSelection *_settings;
	QLabel *_settingsWarning;
	std::shared_ptr<MacroActionPluginState> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};
