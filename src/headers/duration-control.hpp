#pragma once
#include <QWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <chrono>

#include "obs-data.h"

enum class DurationUnit {
	SECONDS,
	MINUTES,
	HOURS,
};

class Duration {
public:
	void Save(obs_data_t *obj, const char *secondsName = "seconds",
		  const char *unitName = "displayUnit");
	void Load(obs_data_t *obj, const char *secondsName = "seconds",
		  const char *unitName = "displayUnit");

	bool DurationReached();
	void Reset();
	std::string ToString();

	double seconds = 0.;
	// only used for UI
	DurationUnit displayUnit = DurationUnit::SECONDS;

private:
	std::chrono::high_resolution_clock::time_point _startTime;
};

class DurationSelection : public QWidget {
	Q_OBJECT
public:
	DurationSelection(QWidget *parent = nullptr,
			  bool showUnitSelection = true);
	void SetValue(double value);
	void SetUnit(DurationUnit u);
	void SetDuration(Duration d);

private slots:
	void _DurationChanged(double value);
	void _UnitChanged(int idx);
signals:
	void DurationChanged(double value); // always reutrn value in seconds
	void UnitChanged(DurationUnit u);

private:
	QDoubleSpinBox *_duration;
	QComboBox *_unitSelection;

	double _unitMultiplier;
};

enum class DurationCondition {
	NONE,
	MORE,
	EQUAL,
	LESS,
};

class DurationConstraint {
public:
	void Save(obs_data_t *obj, const char *condName = "time_constraint",
		  const char *secondsName = "seconds",
		  const char *unitName = "displayUnit");
	void Load(obs_data_t *obj, const char *condName = "time_constraint",
		  const char *secondsName = "seconds",
		  const char *unitName = "displayUnit");
	void SetCondition(DurationCondition cond) { _type = cond; }
	void SetDuration(const Duration &dur) { _dur = dur; }
	void SetValue(double value) { _dur.seconds = value; }
	void SetUnit(DurationUnit u) { _dur.displayUnit = u; }
	DurationCondition GetCondition() { return _type; }
	Duration GetDuration() { return _dur; }
	bool DurationReached();
	void Reset();

private:
	DurationCondition _type = DurationCondition::NONE;
	Duration _dur;
	bool _timeReached = false;
};

class DurationConstraintEdit : public QWidget {
	Q_OBJECT
public:
	DurationConstraintEdit(QWidget *parent = nullptr);
	void SetValue(DurationConstraint &value);
	void SetUnit(DurationUnit u);
	void SetDuration(const Duration &d);

private slots:
	void _ConditionChanged(int value);
	void ToggleClicked();
signals:
	void DurationChanged(double value);
	void UnitChanged(DurationUnit u);
	void ConditionChanged(DurationCondition value);

private:
	void Collapse(bool collapse);

	DurationSelection *_duration;
	QComboBox *_condition;
	QPushButton *_toggle;
};
