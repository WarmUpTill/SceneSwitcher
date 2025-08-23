#pragma once
#include "macro-action-edit.hpp"
#include "file-selection.hpp"

#include <obs.hpp>

#include <QCheckBox>
#include <QLabel>

namespace advss {

class MacroActionPluginState : public MacroAction {
public:
	MacroActionPluginState(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Action {
		STOP,
		NO_MATCH_BEHAVIOUR,
		IMPORT_SETTINGS,
		TERMINATE,
		ENABLE_MACRO_HIGHLIGHTING,
		DISABLE_MACRO_HIGHLIGHTING,
		TOGGLE_MACRO_HIGHLIGHTING,
	};

	Action _action = Action::STOP;
	int _value = 0;
	StringVariable _settingsPath;
	OBSWeakSource _scene;
	bool _confirmShutdown = true;

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
	void ConfirmShutdownChanged(int);

private:
	void SetWidgetVisibility();

	QComboBox *_actions;
	QComboBox *_values;
	QComboBox *_scenes;
	FileSelection *_settings;
	QLabel *_settingsWarning;
	QCheckBox *_confirmShutdown;

	std::shared_ptr<MacroActionPluginState> _entryData;
	bool _loading = true;
};

} // namespace advss
