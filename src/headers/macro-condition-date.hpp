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
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionDate>();
	}

	QDateTime _dateTime;
	QDateTime _dateTime2;
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
	void DateTimeChanged(const QDateTime &datetime);
	void DateTime2Changed(const QDateTime &datetime);
	void IgnoreDateChanged(int state);
	void IgnoreTimeChanged(int state);
	void RepeatChanged(int state);
	void DurationChanged(double seconds);
	void DurationUnitChanged(DurationUnit unit);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_condition;
	QDateTimeEdit *_dateTime;
	QDateTimeEdit *_dateTime2;
	QCheckBox *_ignoreDate;
	QCheckBox *_ignoreTime;
	QCheckBox *_repeat;
	DurationSelection *_duration;
	std::shared_ptr<MacroConditionDate> _entryData;

private:
	void ShowSecondDateSelection(bool visible);
	bool _loading = true;
};
