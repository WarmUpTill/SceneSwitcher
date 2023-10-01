#pragma once
#include "macro-condition-edit.hpp"
#include "variable-text-edit.hpp"
#include "regex-config.hpp"
#include "source-selection.hpp"
#include "filter-selection.hpp"

#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>

namespace advss {

class MacroConditionFilter : public MacroCondition {
public:
	MacroConditionFilter(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionFilter>(m);
	}

	enum class Condition {
		ENABLED,
		DISABLED,
		SETTINGS_MATCH,
		SETTINGS_CHANGED,
	};

	SourceSelection _source;
	FilterSelection _filter;
	Condition _condition = Condition::ENABLED;
	StringVariable _settings = "";
	RegexConfig _regex;

private:
	bool CheckConditionHelper(const OBSWeakSource &);

	std::string _currentSettings;
	static bool _registered;
	static const std::string id;
};

class MacroConditionFilterEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionFilterEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionFilter> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionFilterEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionFilter>(cond));
	}

private slots:
	void SourceChanged(const SourceSelection &);
	void FilterChanged(const FilterSelection &);
	void ConditionChanged(int cond);
	void GetSettingsClicked();
	void SettingsChanged();
	void RegexChanged(RegexConfig);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SourceSelectionWidget *_sources;
	FilterSelectionWidget *_filters;
	QComboBox *_conditions;
	QPushButton *_getSettings;
	VariableTextEdit *_settings;
	RegexConfigWidget *_regex;

	std::shared_ptr<MacroConditionFilter> _entryData;

private:
	void SetSettingsSelectionVisible(bool visible);
	bool _loading = true;
};

} // namespace advss
