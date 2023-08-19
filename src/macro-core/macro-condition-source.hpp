#pragma once
#include "macro-condition-edit.hpp"
#include "variable-text-edit.hpp"
#include "regex-config.hpp"
#include "source-selection.hpp"

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
		SETTINGS_MATCH,
		SETTINGS_CHANGED,
	};

	SourceSelection _source;
	Condition _condition = Condition::ACTIVE;
	StringVariable _settings = "";
	RegexConfig _regex;

private:
	std::string _currentSettings;

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
	void RegexChanged(RegexConfig);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SourceSelectionWidget *_sources;
	QComboBox *_conditions;
	QPushButton *_getSettings;
	VariableTextEdit *_settings;
	RegexConfigWidget *_regex;

	std::shared_ptr<MacroConditionSource> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};

} // namespace advss
