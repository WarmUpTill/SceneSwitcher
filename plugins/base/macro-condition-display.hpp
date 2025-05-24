#pragma once
#include "macro-condition-edit.hpp"
#include "monitor-helpers.hpp"
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
		DISPLAY_WIDTH,
		DISPLAY_HEIGHT,
	};
	enum class CompareMode { EQUALS, LESS, MORE };
	CompareMode _compare = CompareMode::EQUALS;

	void SetCondition(Condition);
	Condition GetCondition() const { return _condition; }

	StringVariable _displayName;
	RegexConfig _regexConf;
	IntVariable _displayCount;
	IntVariable _displayWidth;
	IntVariable _displayHeight;
	bool _useDevicePixelRatio = true;

private:
	void SetupTempVars();

	Condition _condition = Condition::DISPLAY_NAME;

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
	void CompareModeChanged(int);
	void DisplayNameChanged(const QString &);
	void RegexChanged(const RegexConfig &conf);
	void DisplayCountChanged(const NumberVariable<int> &);
	void DisplayWidthChanged(const NumberVariable<int> &);
	void DisplayHeightChanged(const NumberVariable<int> &);
	void UseDevicePixelRatioChanged(int state);

private:
	void SetWidgetVisibility();

	QComboBox *_conditions;
	QComboBox *_compareModes;
	MonitorSelectionWidget *_displays;
	RegexConfigWidget *_regex;
	VariableSpinBox *_displayCount;
	VariableSpinBox *_displayWidth;
	VariableSpinBox *_displayHeight;
	QCheckBox *_useDevicePixelRatio;

	std::shared_ptr<MacroConditionDisplay> _entryData;
	bool _loading = true;
};

} // namespace advss
