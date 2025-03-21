#pragma once
#include "macro-action-edit.hpp"
#include "regex-config.hpp"
#include "scene-selection.hpp"
#include "source-selection.hpp"
#include "variable-line-edit.hpp"

namespace advss {

class MacroActionProjector : public MacroAction {
public:
	MacroActionProjector(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();
	void SetMonitor(int);
	int GetMonitor() const;

	enum class Action {
		OPEN,
		CLOSE,
	};

	enum class Type {
		SOURCE,
		SCENE,
		PREVIEW,
		PROGRAM,
		MULTIVIEW,
	};

	Action _action = Action::OPEN;
	Type _type = Type::SCENE;
	SourceSelection _source;
	SceneSelection _scene;
	bool _fullscreen = true;
	StringVariable _projectorWindowName = "Windowed Projector";
	RegexConfig _regex = RegexConfig::PartialMatchRegexConfig(true);

private:
	bool MonitorSetupChanged() const;

	int _monitor = -1;
	// Only used to detect display setup changes
	std::string _monitorName = "";

	static bool _registered;
	static const std::string id;
};

class MacroActionProjectorEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionProjectorEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionProjector> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionProjectorEdit(
			parent, std::dynamic_pointer_cast<MacroActionProjector>(
					action));
	}

private slots:
	void ActionChanged(int value);
	void WindowTypeChanged(int value);
	void TypeChanged(int value);
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SourceSelection &);
	void MonitorChanged(int value);
	void ProjectorWindowNameChanged();
	void RegexChanged(const RegexConfig &);

private:
	void SetWidgetLayout();
	void SetWidgetVisibility();

	QComboBox *_actions;
	QComboBox *_types;
	QComboBox *_windowTypes;
	SceneSelectionWidget *_scenes;
	SourceSelectionWidget *_sources;
	QComboBox *_monitors;
	VariableLineEdit *_projectorWindowName;
	RegexConfigWidget *_regex;
	QHBoxLayout *_layout;

	std::shared_ptr<MacroActionProjector> _entryData;
	bool _loading = true;
};

} // namespace advss
