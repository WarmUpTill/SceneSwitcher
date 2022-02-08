#pragma once
#include "macro.hpp"
#include "duration-control.hpp"

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QComboBox>

enum class DateCondition {
	AT,
	AFTER,
	BEFORE,
	BETWEEN,
};

class MacroConditionDate : public MacroCondition {
public:
	MacroConditionDate(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionDate>(m);
	}

	QDateTime _dateTime = QDateTime::currentDateTime();
	QDateTime _dateTime2 = QDateTime::currentDateTime();
	bool _ignoreDate = false;
	bool _ignoreTime = false;
	bool _repeat = false;
	Duration _duration;
	DateCondition _condition = DateCondition::AT;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionDateEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionDateEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionDate> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionDateEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionDate>(cond));
	}

private slots:
	void ConditionChanged(int cond);
	void DateChanged(const QDate &date);
	void TimeChanged(const QTime &time);
	void Date2Changed(const QDate &date);
	void Time2Changed(const QTime &time);
	void IgnoreDateChanged(int state);
	void IgnoreTimeChanged(int state);
	void RepeatChanged(int state);
	void DurationChanged(double seconds);
	void DurationUnitChanged(DurationUnit unit);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_condition;
	QDateEdit *_date;
	QTimeEdit *_time;
	QLabel *_separator;
	QDateEdit *_date2;
	QTimeEdit *_time2;
	QCheckBox *_ignoreDate;
	QCheckBox *_ignoreTime;
	QCheckBox *_repeat;
	DurationSelection *_duration;
	std::shared_ptr<MacroConditionDate> _entryData;

private:
	void SetWidgetStatus();
	void ShowSecondDateSelection(bool visible);
	bool _loading = true;
};
