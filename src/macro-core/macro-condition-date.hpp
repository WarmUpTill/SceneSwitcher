#pragma once
#include "macro.hpp"
#include "duration-control.hpp"

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QComboBox>
#include <QTimer>

class MacroConditionDate : public MacroCondition {
public:
	MacroConditionDate(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionDate>(m);
	}

	void SetDate1(const QDate &date);
	void SetDate2(const QDate &date);
	void SetTime1(const QTime &time);
	void SetTime2(const QTime &time);
	QDateTime GetDateTime1() const;
	QDateTime GetDateTime2() const;
	QDateTime GetNextMatchDateTime() const;

	enum class Day {
		ANY = 0,
		MONDAY,
		TUESDAY,
		WEDNESDAY,
		THURSDAY,
		FRIDAY,
		SATURDAY,
		SUNDAY,
	};

	enum class Condition {
		AT,
		AFTER,
		BEFORE,
		BETWEEN,
		PATTERN,
	};

	Day _dayOfWeek = Day::ANY;
	bool _ignoreDate = false;
	bool _ignoreTime = false;
	bool _repeat = false;
	bool _updateOnRepeat = true;
	Duration _duration;
	Condition _condition = Condition::AT;
	bool _dayOfWeekCheck = true;
	std::string _pattern = ".... .. .. .. .. ..";

private:
	bool CheckDayOfWeek(int64_t);
	bool CheckRegularDate(int64_t);
	bool CheckBetween(const QDateTime &now);
	bool CheckPattern(QDateTime now, int64_t secondsSinceLastCheck);

	QDateTime _dateTime = QDateTime::currentDateTime();
	QDateTime _dateTime2 = QDateTime::currentDateTime();
	// Used depending whether or not _updateOnRepeat is set
	QDateTime _origDateTime = QDateTime::currentDateTime();
	QDateTime _origDateTime2 = QDateTime::currentDateTime();

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
	void DayOfWeekChanged(int day);
	void ConditionChanged(int cond);
	void DateChanged(const QDate &date);
	void TimeChanged(const QTime &time);
	void Date2Changed(const QDate &date);
	void Time2Changed(const QTime &time);
	void IgnoreDateChanged(int state);
	void IgnoreTimeChanged(int state);
	void RepeatChanged(int state);
	void UpdateOnRepeatChanged(int state);
	void DurationChanged(double seconds);
	void DurationUnitChanged(DurationUnit unit);
	void AdvancedSettingsToggleClicked();
	void ShowNextMatch();
	void UpdateCurrentTime();
	void PatternChanged();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_weekCondition;
	QComboBox *_dayOfWeek;
	QCheckBox *_ignoreWeekTime;
	QTimeEdit *_weekTime;

	QComboBox *_condition;
	QDateEdit *_date;
	QTimeEdit *_time;
	QLabel *_separator;
	QDateEdit *_date2;
	QTimeEdit *_time2;
	QCheckBox *_ignoreDate;
	QCheckBox *_ignoreTime;
	QCheckBox *_repeat;
	QLabel *_nextMatchDate;
	QCheckBox *_updateOnRepeat;
	DurationSelection *_duration;
	QLineEdit *_pattern;
	QLabel *_currentDate;

	QPushButton *_advancedSettingsTooggle;
	QHBoxLayout *_simpleLayout;
	QHBoxLayout *_advancedLayout;
	QVBoxLayout *_repeatLayout;
	QHBoxLayout *_repeatUpdateLayout;
	QHBoxLayout *_patternLayout;

	std::shared_ptr<MacroConditionDate> _entryData;

private:
	void SetupSimpleView();
	void SetupAdvancedView();
	void SetupPatternView();
	void SetWidgetStatus();
	void ShowFirstDateSelection(bool visible);
	void ShowSecondDateSelection(bool visible);
	QTimer _timer;
	bool _loading = true;
};
