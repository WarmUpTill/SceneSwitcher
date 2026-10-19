#pragma once
#include "macro-action-variable-group.hpp"
#include "help-icon.hpp"
#include "variable-line-edit.hpp"
#include "variable-spinbox.hpp"

#include <QDateTime>

namespace advss {

class VariableActionTime : public VariableActionGroup {
public:
	std::unique_ptr<VariableActionGroup> Copy() const override;
	void Save(obs_data_t *, const MacroActionVariable &) const override;
	void Load(obs_data_t *, MacroActionVariable &) override;
	void ResolveVariables() override;
	bool Perform(MacroActionVariable &, MacroActionVariable::Action,
		     Variable &) override;

	enum class TimeUnit { SECONDS, MILLISECONDS };
	enum class OffsetUnit {
		SECONDS,
		MINUTES,
		HOURS,
		DAYS,
		WEEKS,
		MONTHS,
		YEARS,
	};

	StringVariable _dateFormat = "yyyy-MM-dd hh:mm:ss";
	TimeUnit _timeUnit = TimeUnit::SECONDS;
	IntVariable _dateOffset = 1;
	OffsetUnit _dateOffsetUnit = OffsetUnit::DAYS;

private:
	void SetToTimeSinceSceneChange(Variable &) const;
	QDateTime GetDateTimeWithOffset() const;
};

class VariableActionTimeEdit : public VariableActionGroupEdit {
	Q_OBJECT

public:
	VariableActionTimeEdit(QWidget *parent,
			       std::shared_ptr<MacroActionVariable> entryData);
	static VariableActionGroupEdit *
	Create(QWidget *parent, std::shared_ptr<MacroActionVariable> entryData);
	std::unordered_map<std::string, QWidget *> GetInlineWidgets() override;
	void UpdateEntryData() override;
	void SetWidgetVisibility(MacroActionVariable::Action) override;

private slots:
	void DateFormatChanged();
	void TimeUnitChanged(int);
	void DateOffsetChanged(const NumberVariable<int> &);
	void DateOffsetUnitChanged(int);

private:
	VariableActionTime *_data;

	VariableLineEdit *_dateFormat;
	HelpIcon *_dateFormatHelp;
	QComboBox *_timeUnit;
	VariableSpinBox *_dateOffset;
	QComboBox *_dateOffsetUnit;
};

} // namespace advss
