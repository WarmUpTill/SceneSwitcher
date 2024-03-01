#pragma once
#include "macro-action-edit.hpp"
#include "variable-text-edit.hpp"
#include "source-selection.hpp"
#include "filter-selection.hpp"
#include "source-setting.hpp"

#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>

namespace advss {

class MacroActionFilter : public MacroAction {
public:
	MacroActionFilter(Macro *m) : MacroAction(m) {}
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
		ENABLE,
		DISABLE,
		TOGGLE,
		SETTINGS,
	};

	enum class SettingsInputMethod {
		INDIVIDUAL_MANUAL,
		INDIVIDUAL_TEMPVAR,
		JSON_STRING,
	};
	SettingsInputMethod _settingsInputMethod =
		SettingsInputMethod::INDIVIDUAL_MANUAL;

	SourceSelection _source;
	FilterSelection _filter;
	Action _action = Action::ENABLE;
	StringVariable _settingsString = "";
	TempVariableRef _tempVar;
	StringVariable _manualSettingValue = "";
	SourceSetting _setting;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionFilterEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionFilterEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionFilter> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionFilterEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionFilter>(action));
	}

private slots:
	void SourceChanged(const SourceSelection &);
	void FilterChanged(const FilterSelection &);
	void ActionChanged(int value);
	void GetSettingsClicked();
	void SettingsStringChanged();
	void SettingsInputMethodChanged(int);
	void SelectionChanged(const TempVariableRef &);
	void SelectionChanged(const SourceSetting &);
	void ManualSettingsValueChanged();
	void RefreshVariableSourceSelectionValue();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SourceSelectionWidget *_sources;
	FilterSelectionWidget *_filters;
	QComboBox *_actions;
	QPushButton *_getSettings;
	QHBoxLayout *_settingsLayout;
	QComboBox *_settingsInputMethods;
	VariableTextEdit *_manualSettingValue;
	TempVariableSelection *_tempVars;
	SourceSettingSelection *_filterSettings;
	VariableTextEdit *_settingsString;
	QPushButton *_refreshSettingSelection;

	std::shared_ptr<MacroActionFilter> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};

} // namespace advss
