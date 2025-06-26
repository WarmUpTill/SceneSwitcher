#pragma once
#include "macro-condition-edit.hpp"
#include "variable-text-edit.hpp"
#include "regex-config.hpp"
#include "source-selection.hpp"
#include "source-setting.hpp"

#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>

namespace advss {

class MacroConditionSource : public MacroCondition {
public:
	MacroConditionSource(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionSource>(m);
	}

	enum class Condition {
		ACTIVE,
		SHOWING,
		ALL_SETTINGS_MATCH,
		SETTINGS_CHANGED,
		INDIVIDUAL_SETTING_MATCH,
		INDIVIDUAL_SETTING_CHANGED,
		HEIGHT,
		WIDTH,
		INDIVIDUAL_SETTING_LIST_ENTRY_MATCH,
	};
	void SetCondition(Condition);
	Condition GetCondition() const { return _condition; }

	enum class SizeComparision { LESS, EQUAL, MORE };

	SourceSelection _source;
	StringVariable _settings = "";
	SourceSetting _setting;
	RegexConfig _regex;
	IntVariable _size;
	SizeComparision _comparison = SizeComparision::EQUAL;
	bool _includeDefaults = false;

private:
	void SetupTempVars();

	Condition _condition = Condition::ACTIVE;
	std::optional<std::string> _currentSettings;
	std::string _currentSettingsValue;

	static bool _registered;
	static const std::string id;
};

class MacroConditionSourceEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSourceEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionSource> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSourceEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionSource>(cond));
	}

private slots:
	void SourceChanged(const SourceSelection &);
	void ConditionChanged(int cond);
	void GetSettingsClicked();
	void SettingsChanged();
	void RegexChanged(const RegexConfig &);
	void SettingSelectionChanged(const SourceSetting &);
	void RefreshVariableSourceSelectionValue();
	void SizeChanged(const NumberVariable<int> &value);
	void CompareMethodChanged(int);
	void IncludeDefaultsChanged(int);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SourceSelectionWidget *_sources;
	QComboBox *_conditions;
	QPushButton *_getSettings;
	VariableTextEdit *_settings;
	RegexConfigWidget *_regex;
	SourceSettingSelection *_settingSelection;
	QPushButton *_refreshSettingSelection;
	VariableSpinBox *_size;
	QComboBox *_sizeCompareMethods;
	QCheckBox *_includeDefaults;

	std::shared_ptr<MacroConditionSource> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};

} // namespace advss
