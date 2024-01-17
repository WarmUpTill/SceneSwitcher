#pragma once
#include "macro-condition-edit.hpp"
#include "variable-line-edit.hpp"
#include "variable-spinbox.hpp"
#include "regex-config.hpp"

#include <QWidget>
#include <QComboBox>

namespace advss {

class MacroConditionDisplay : public MacroCondition {
public:
	MacroConditionDisplay(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionDisplay>(m);
	}

	enum class Condition {
		DISPLAY_NAME,
		DISPLAY_COUNT,
	};
	Condition _condition = Condition::DISPLAY_NAME;
	StringVariable _displayName;
	RegexConfig _regexConf;
	IntVariable _displayCount;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionDisplayEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionDisplayEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionDisplay> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionDisplayEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionDisplay>(cond));
	}

private slots:
	void ConditionChanged(int);
	void DisplayNameChanged(const QString &);
	void RegexChanged(const RegexConfig &conf);
	void DisplayCountChanged(const NumberVariable<int> &);

protected:
	QComboBox *_conditions;
	QComboBox *_displays;
	RegexConfigWidget *_regex;
	VariableSpinBox *_displayCount;
	std::shared_ptr<MacroConditionDisplay> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
